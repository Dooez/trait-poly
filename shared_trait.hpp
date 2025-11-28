#pragma once
#ifndef TRP_GODBOLT
#include "trait_impl.hpp"
#endif

#include <atomic>
#include <memory>
namespace trp {
namespace detail {
using arc_t = std::atomic<uint64_t>;

template<typename Impl>
consteval auto get_align() {
    static_assert(alignof(void*) == alignof(uZ));
    static_assert(alignof(arc_t) >= alignof(void*));
    return std::max({alignof(Impl), alignof(void*), alignof(arc_t)});
};
struct signed_offsets {
    static constexpr iZ memstart   = -sizeof(arc_t) - sizeof(void*) * 2 - sizeof(uZ);
    static constexpr iZ memsize    = -sizeof(arc_t) - sizeof(void*) - sizeof(uZ);
    static constexpr iZ dtor       = -sizeof(arc_t) - sizeof(void*);
    static constexpr iZ arc        = -sizeof(arc_t);
    static constexpr iZ impl       = 0;
    static constexpr iZ ctrl_block = memstart;
};

template<any_trait Trait, typename Impl, typename Allocator>
struct alignas(get_align<Impl>()) ctrl_block {
    static constexpr auto ialign   = get_align<Impl>();
    static constexpr auto hdr_size = sizeof(void*) * 2 + sizeof(uZ) + sizeof(arc_t);
    static constexpr auto misalign = (hdr_size + ialign - 1) / ialign * ialign - hdr_size;
    using dtor_ptr                 = void (*)(void*);

    std::byte* memstart_;
    uZ         n_;
    dtor_ptr   destructor_ptr_;
    arc_t      counter_{};
    Impl       impl_;

    [[no_unique_address]] Allocator allocator_;

    static void destroy(void* ctrl_ptr) {
        auto& ctrl      = *static_cast<ctrl_block*>(ctrl_ptr);
        auto  allocator = ctrl.allocator_;
        auto  memstart  = ctrl.memstart_;
        auto  n         = ctrl.n_;
        ctrl.~ctrl_block();
        std::allocator_traits<Allocator>::deallocate(allocator, memstart, n);
    }
};

struct shared_manager : trait_impl_manager {
    shared_manager(const efptr* vtable_begin, void* obj_ptr)
    : trait_impl_manager{.vtable_begin{vtable_begin}, .obj_ptr{obj_ptr}} {
        increment();
    };


    auto obj() const -> std::byte* {
        return static_cast<std::byte*>(obj_ptr);
    }

    void increment() const {
        if (obj_ptr == nullptr)
            return;
        auto& counter = *reinterpret_cast<arc_t*>(obj() + signed_offsets::arc);
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    void decrement() const {
        if (obj_ptr == nullptr)
            return;
        auto& counter = *reinterpret_cast<arc_t*>(obj() + signed_offsets::arc);
        if (counter.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            using dtor_t         = void (*)(void*);
            auto* dtor           = *reinterpret_cast<dtor_t*>(obj() + signed_offsets::dtor);
            auto* ctrl_block_ptr = *reinterpret_cast<void**>(obj() + signed_offsets::ctrl_block);
            dtor(ctrl_block_ptr);
        };
    }
    shared_manager() = default;
    shared_manager(const shared_manager& other) {
        other.increment();
        vtable_begin = other.vtable_begin;
        obj_ptr      = other.obj_ptr;
    }
    shared_manager(shared_manager&& other) {
        vtable_begin       = other.vtable_begin;
        obj_ptr            = other.obj_ptr;
        other.vtable_begin = nullptr;
        other.obj_ptr      = nullptr;
    }
    shared_manager& operator=(const shared_manager& other) {
        decrement();
        other.increment();
        vtable_begin = other.vtable_begin;
        obj_ptr      = other.obj_ptr;
        return *this;
    }
    shared_manager& operator=(shared_manager&& other) {
        vtable_begin       = other.vtable_begin;
        obj_ptr            = other.obj_ptr;
        other.vtable_begin = nullptr;
        other.obj_ptr      = nullptr;
        return *this;
    }
    ~shared_manager() {
        decrement();
    }
};
}    // namespace detail

template<any_trait T>
class shared_trait
: public detail::trait_impl<T>
, private detail::shared_manager {
    static_assert(detail::validate_method_offsets<detail::trait_impl<T>>());

public:
    shared_trait() = default;
    shared_trait(detail::shared_manager manager)
    : detail::shared_manager(std::move(manager)) {};

    operator bool() {
        return obj_ptr != nullptr;
    }
};
template<any_trait Trait, implements_trait<Trait> Impl, typename Alloc, typename... Args>
auto allocate_shared_trait(const Alloc& allocator, Args&&... args) {
    static_assert(sizeof(shared_trait<Trait>) == sizeof(detail::shared_manager));
    static_assert(
        stdr::all_of(std::meta::bases_of(^^shared_trait<Trait>, std::meta::access_context::unchecked()),
                     [](auto info) { return std::meta::offset_of(info).bytes == 0; }));
    using alloc        = std::allocator_traits<Alloc>::template rebind_alloc<std::byte>;
    using ctrl_block   = detail::ctrl_block<Trait, Impl, alloc>;
    using alloc_traits = std::allocator_traits<alloc>;
    auto new_allocator = static_cast<alloc>(allocator);
    const auto [ptr, n] =
        alloc_traits::allocate_at_least(new_allocator, sizeof(ctrl_block) + alignof(ctrl_block));
    const auto destructor_ptr = static_cast<void (*)(void*)>(&ctrl_block::destroy);
    auto       new_n          = n;
    auto       new_ptr        = static_cast<void*>(ptr);
    auto       ctrl_ptr       = std::align(alignof(ctrl_block), sizeof(ctrl_block), new_ptr, new_n);
    if (ctrl_ptr == nullptr) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw std::runtime_error("Could not allocate aligned storage");
    }
    try {
        auto cptr       = new (ctrl_ptr) ctrl_block{.memstart_{ptr},
                                                    .n_{n},
                                                    .destructor_ptr_{destructor_ptr},
                                                    .impl_{std::forward<Args>(args)...},
                                                    .allocator_{std::move(new_allocator)}};
        auto impl_ptr   = &(cptr->impl_);
        auto vtable_ptr = detail::trait_vtable<Trait, Impl>::value.data();
        using efptr     = void*;
        return shared_trait<Trait>(
            detail::shared_manager(reinterpret_cast<const efptr*>(vtable_ptr), static_cast<void*>(impl_ptr)));
    } catch (...) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw;
    }
}

template<any_trait Trait, implements_trait<Trait> Impl, typename... Args>
auto make_shared_trait(Args&&... args) {
    return allocate_shared_trait<Trait, Impl>(std::allocator<std::byte>{}, std::forward<Args>(args)...);
}
}    // namespace trp
