#pragma once
#ifndef TRP_GODBOLT
#include "allocator_ctrl_block.hpp"
#include "trait_impl.hpp"
#endif

#include <atomic>
#include <memory>
#include <stdexcept>
namespace trp {
namespace detail {
using arc_t = std::atomic<uint64_t>;
}    // namespace detail

template<any_trait Trait>
class shared_trait_ptr {
    using ctrl_header = detail::ctrl_header<detail::arc_t>;
    trait_ref<Trait> trait_ref_{};
    ctrl_header*     ctrl_ptr_{};

    shared_trait_ptr(trait_ref<Trait> trait_ref, ctrl_header* ctrl_ptr)
    : trait_ref_(trait_ref)
    , ctrl_ptr_(ctrl_ptr) {
        // increment is external
    };

    template<implements_trait<Trait> Impl>
    shared_trait_ptr(Impl* obj_ptr, ctrl_header* ctrl_ptr)
    : trait_ref_(*obj_ptr)
    , ctrl_ptr_(ctrl_ptr) {
        increment();
    };
    [[nodiscard]] auto get_ctrl_ptr() const -> ctrl_header* {
        return ctrl_ptr_;
    }
    [[nodiscard]] bool holds_value() const {
        return trait_ref_.holds_value();
    }

    void release() {
        trait_ref_.release();
        ctrl_ptr_ = nullptr;
    }

    void increment() const {
        if (not holds_value())
            return;
        ctrl_ptr_->counter.fetch_add(1, std::memory_order_relaxed);
    }
    void decrement() {
        if (not holds_value())
            return;
        if (ctrl_ptr_->counter.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            ctrl_ptr_->destructor_ptr_(ctrl_ptr_);
            release();
        };
    }

public:
    explicit operator bool() const {
        return holds_value();
    }
    auto operator->(this auto&& self) -> auto* {
        return &self.trait_ref_;
    }
    auto operator*(this auto&& self) -> auto& {
        return self.trait_ref_;
    }
    ~shared_trait_ptr() {
        decrement();
    }

    shared_trait_ptr() = default;
    shared_trait_ptr(const shared_trait_ptr& other) noexcept {
        other.increment();
        trait_ref_.rebind(other.trait_ref_);
        ctrl_ptr_ = other.ctrl_ptr_;
    }
    shared_trait_ptr(shared_trait_ptr&& other) noexcept
    : trait_ref_{other.trait_ref_}
    , ctrl_ptr_(other.ctrl_ptr_) {
        other.release();
    }
    shared_trait_ptr& operator=(const shared_trait_ptr& other) noexcept {
        if (this == &other)
            return *this;
        decrement();
        other.increment();
        trait_ref_.rebind(other.trait_ref_);
        ctrl_ptr_ = other.ctrl_ptr_;
        return *this;
    }
    shared_trait_ptr& operator=(shared_trait_ptr&& other) noexcept {
        if (this == &other)
            return *this;
        decrement();
        trait_ref_.rebind(other.trait_ref_);
        ctrl_ptr_ = other.ctrl_ptr_;
        other.release();
        return *this;
    }

private:
    template<any_trait T, implements_trait<T>, typename Alloc, typename... Args>
    friend auto allocate_shared_trait(const Alloc&, Args&&...);

    template<typename S, typename T>
        requires explicit_supertrait_of<S, T>
    friend auto trait_cast(shared_trait_ptr<T> ptr);
    template<explicit_supertrait_of<Trait> Supertrait>
    [[nodiscard]] friend auto upcast(shared_trait_ptr ptr) -> shared_trait_ptr<Supertrait> {
        auto new_ptr =
            shared_trait_ptr<Supertrait>(trait_cast<Supertrait>(ptr.trait_ref_), ptr.get_ctrl_ptr());
        ptr.release();
        return new_ptr;
    }
    template<any_trait>
    friend class shared_trait_ptr;
};

template<typename Supertrait, typename Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
auto trait_cast(shared_trait_ptr<Trait> ptr) -> shared_trait_ptr<Supertrait> {
    return upcast<Supertrait>(std::move(ptr));
}

template<typename Impl, any_trait Trait>
    requires implements_trait<Impl, Trait>
auto is_holding_type(const shared_trait_ptr<Trait>& ptr) -> bool {
    return is_holding_type<Impl>(*ptr);
}


template<any_trait Trait, implements_trait<Trait> Impl, typename Alloc, typename... Args>
[[nodiscard]] auto allocate_shared_trait(const Alloc& allocator, Args&&... args) {
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
        const auto impl_ptr = &(cptr->impl_);
        return shared_trait_ptr<Trait>(impl_ptr, cptr);
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
