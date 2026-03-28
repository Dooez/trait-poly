#pragma once
#ifndef TRP_GODBOLT
#include "allocator_ctrl_block.hpp"
#include "trait_impl.hpp"
#endif

#include <atomic>
#include <memory>
namespace trp {
namespace detail {
using arc_t = std::atomic<uint64_t>;

template<any_trait Trait>
struct shared_trait_ptr_impl : public detail::trait_impl<Trait> {
    using impl_t = detail::trait_impl<Trait>;
    using vtable = impl_t::vtable_t;
    ctrl_header<arc_t>* ctrl_ptr_{};

    shared_trait_ptr_impl(const vtable* vtable_ptr, void* obj_ptr, ctrl_header<arc_t>* ctrl_ptr)
    : impl_t{vtable_ptr, obj_ptr}
    , ctrl_ptr_(ctrl_ptr) {
        increment();
    };

    void increment() const {
        if (this->obj_ptr_ == nullptr)
            return;
        ctrl_ptr_->counter.fetch_add(1, std::memory_order_relaxed);
    }
    void decrement() const {
        if (this->obj_ptr_ == nullptr)
            return;
        if (ctrl_ptr_->counter.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            ctrl_ptr_->destructor_ptr_(ctrl_ptr_);
        };
    }
    shared_trait_ptr_impl() = default;
    shared_trait_ptr_impl(const shared_trait_ptr_impl& other) noexcept {
        other.increment();
        this->vtable_ptr_ = other.vtable_ptr_;
        this->obj_ptr_    = other.obj_ptr_;
        this->ctrl_ptr_   = other.ctrl_ptr_;
    }
    shared_trait_ptr_impl(shared_trait_ptr_impl&& other) noexcept
    : impl_t{other.vtable_ptr_, other.obj_ptr_}
    , ctrl_ptr_(other.ctrl_ptr_) {
        other.vtable_ptr_ = nullptr;
        other.obj_ptr_    = nullptr;
        other.ctrl_ptr_   = nullptr;
    }
    shared_trait_ptr_impl& operator=(const shared_trait_ptr_impl& other) noexcept {
        if (this == &other)
            return *this;
        decrement();
        other.increment();
        this->vtable_ptr_ = other.vtable_ptr_;
        this->obj_ptr_    = other.obj_ptr_;
        return *this;
    }
    shared_trait_ptr_impl& operator=(shared_trait_ptr_impl&& other) noexcept {
        this->vtable_ptr_ = other.vtable_ptr_;
        this->obj_ptr_    = other.obj_ptr_;
        other.vtable_ptr_ = nullptr;
        other.obj_ptr_    = nullptr;
        return *this;
    }
    ~shared_trait_ptr_impl() {
        decrement();
    }
};
}    // namespace detail

template<any_trait Trait>
class shared_trait_ptr : public detail::shared_trait_ptr_impl<Trait> {
    template<any_trait T, implements_trait<T>, typename Alloc, typename... Args>
    friend auto allocate_shared_trait(const Alloc&, Args&&...);

    explicit shared_trait_ptr(detail::shared_trait_ptr_impl<Trait> manager)
    : detail::shared_trait_ptr_impl<Trait>(std::move(manager)) {};

public:
    shared_trait_ptr() = default;
    explicit operator bool() {
        return this->_impl_manager.obj_ptr_ != nullptr;
    }
};
template<any_trait Trait, implements_trait<Trait> Impl, typename Alloc, typename... Args>
auto allocate_shared_trait(const Alloc& allocator, Args&&... args) {
    using alloc        = std::allocator_traits<Alloc>::template rebind_alloc<std::byte>;
    using ctrl_block   = detail::ctrl_block<Impl, alloc, detail::arc_t>;
    using alloc_traits = std::allocator_traits<alloc>;
    auto new_allocator = static_cast<alloc>(allocator);
    const auto [ptr, n] =
        alloc_traits::allocate_at_least(new_allocator, sizeof(ctrl_block) + alignof(ctrl_block) - 1);
    auto  tmp_n    = n;
    auto* tmp_ptr  = static_cast<void*>(ptr);
    auto  ctrl_ptr = std::align(alignof(ctrl_block), sizeof(ctrl_block), tmp_ptr, tmp_n);
    if (ctrl_ptr == nullptr) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw std::runtime_error("Could not align allocated storage");
    }
    try {
        auto cptr = new (ctrl_ptr) ctrl_block(ptr, n, std::move(new_allocator), std::forward<Args>(args)...);
        const auto impl_ptr   = &(cptr->impl_);
        const auto vtable_ptr = &detail::trait_vtable_for<Trait, Impl>::value;
        return shared_trait_ptr<Trait>(detail::shared_trait_ptr_impl<Trait>(vtable_ptr, impl_ptr, cptr));
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
