#pragma once
#ifndef TRP_GODBOLT
#include "trait_impl.hpp"
#endif

#include <atomic>
#include <memory>
namespace trp {
namespace detail {
using arc_t = std::atomic<uint64_t>;

struct ctrl_header {
    using dtor_ptr = void (*)(void*);
    std::byte* memstart;
    uZ         n;
    void*      ctrl_block_ptr;
    dtor_ptr   destructor_ptr;
    arc_t      counter{};
};
template<typename Impl>
consteval auto get_align() {
    return std::max({alignof(Impl), alignof(ctrl_header)});
};

template<typename Impl, typename Allocator>
struct alignas(get_align<Impl>()) ctrl_block {
    static constexpr auto impl_align = get_align<Impl>();
    static constexpr auto hdr_size   = sizeof(ctrl_header);
    static constexpr auto misalign   = (hdr_size + impl_align - 1) / impl_align * impl_align - hdr_size;

    std::array<std::byte, misalign> align_array_;
    ctrl_header                     header_;
    Impl                            impl_;
    [[no_unique_address]] Allocator allocator_;

    static_assert((std::meta::offset_of(^^impl_).bytes - std::meta::offset_of(^^header_).bytes) ==
                  sizeof(ctrl_header));

    static void destroy(void* ctrl_ptr) {
        auto& ctrl      = *static_cast<ctrl_block*>(ctrl_ptr);
        auto  allocator = ctrl.allocator_;
        auto  memstart  = ctrl.header_.memstart;
        auto  n         = ctrl.header_.n;
        ctrl.~ctrl_block();
        std::allocator_traits<Allocator>::deallocate(allocator, memstart, n);
    }
};

struct shared_manager : trait_impl_manager {
    shared_manager(const efptr* vtable_begin, void* obj_ptr)
    : trait_impl_manager{.vtable_begin = vtable_begin, .obj_ptr = obj_ptr} {
        increment();
    };

    auto header() const -> ctrl_header& {
        return *(static_cast<ctrl_header*>(obj_ptr) - 1);
    }

    void increment() const {
        if (obj_ptr == nullptr)
            return;
        header().counter.fetch_add(1, std::memory_order_relaxed);
    }
    void decrement() const {
        if (obj_ptr == nullptr)
            return;
        if (header().counter.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            auto* dtor           = header().destructor_ptr;
            auto* ctrl_block_ptr = header().ctrl_block_ptr;
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
    static_assert(stdr::all_of(std::meta::bases_of(^^shared_trait<Trait>, ctx_unchecked),
                               [](auto info) { return std::meta::offset_of(info).bytes == 0; }));
    using alloc        = std::allocator_traits<Alloc>::template rebind_alloc<std::byte>;
    using ctrl_block   = detail::ctrl_block<Impl, alloc>;
    using alloc_traits = std::allocator_traits<alloc>;
    auto new_allocator = static_cast<alloc>(allocator);
    const auto [ptr, n] =
        alloc_traits::allocate_at_least(new_allocator, sizeof(ctrl_block) + alignof(ctrl_block) - 1);
    const auto destructor_ptr = &ctrl_block::destroy;
    auto       new_n          = n;
    auto       new_ptr        = static_cast<void*>(ptr);
    auto       ctrl_ptr       = std::align(alignof(ctrl_block), sizeof(ctrl_block), new_ptr, new_n);
    if (ctrl_ptr == nullptr) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw std::runtime_error("Could not align allocated storage");
    }
    try {
        auto cptr = new (ctrl_ptr) ctrl_block{
            .header_{
                     .memstart{ptr},
                     .n{n},
                     .ctrl_block_ptr{ctrl_ptr},
                     .destructor_ptr{destructor_ptr},
                     },
            .impl_{std::forward<Args>(args)...},
            .allocator_{std::move(new_allocator)}
        };
        const auto impl_ptr   = &(cptr->impl_);
        const auto vtable_ptr = detail::trait_vtable<Trait, Impl>::value.data();
        return shared_trait<Trait>(detail::shared_manager(vtable_ptr, impl_ptr));
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
