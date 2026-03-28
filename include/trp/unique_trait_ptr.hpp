#pragma once
#ifndef TRP_GODBOLT
#include "allocator_ctrl_block.hpp"
#include "trait_impl.hpp"
#endif

#include <memory>

namespace trp {
template<any_trait Trait>
class unique_trait_ptr : public detail::trait_impl<Trait> {
    using impl_t = detail::trait_impl<Trait>;
    using vtable = impl_t::vtable_t;

    template<any_trait>
    friend class alloc_unique_trait_ptr;

public:
    template<implements_trait<Trait> Impl>
    explicit unique_trait_ptr(Impl* obj_ptr)
    : impl_t{&detail::trait_vtable_for<Trait, Impl>::value, obj_ptr} {};

    unique_trait_ptr() = default;
    unique_trait_ptr(unique_trait_ptr&& other) noexcept
    : impl_t{other.vtable_ptr_, other.obj_ptr_} {
        other.obj_ptr_ = nullptr;
    }
    unique_trait_ptr& operator=(unique_trait_ptr&& other) noexcept {
        if (this->obj_ptr_ != nullptr)
            this->vtable_ptr_->default_delete(this->obj_ptr_);
        this->vtable_ptr_ = other.vtable_ptr_;
        this->obj_ptr_    = other.obj_ptr_;
        other.obj_ptr_    = nullptr;
        return *this;
    };

    unique_trait_ptr(const unique_trait_ptr&)            = delete;
    unique_trait_ptr& operator=(const unique_trait_ptr&) = delete;

    ~unique_trait_ptr() {
        if (this->obj_ptr_ != nullptr)
            this->vtable_ptr_->default_delete(this->obj_ptr_);
    }
};

template<any_trait Trait, implements_trait<Trait> Impl, typename... Args>
auto make_unique_trait(Args&&... args) -> unique_trait_ptr<Trait> {
    return unique_trait_ptr<Trait>(new Impl(std::forward<Args>(args)...));
}

template<any_trait Trait>
class alloc_unique_trait_ptr : public detail::trait_impl<Trait> {
    using impl_t      = detail::trait_impl<Trait>;
    using vtable      = impl_t::vtable_t;
    using ctrl_header = detail::ctrl_header<>;
    ctrl_header* ctrl_ptr_{};

    template<any_trait T, implements_trait<T>, typename Alloc, typename... Args>
    friend auto allocate_unique_trait(const Alloc& allocator, Args&&... args);

    alloc_unique_trait_ptr(const vtable* vtable_ptr, void* obj_ptr, ctrl_header* ctrl_ptr)
    : impl_t{vtable_ptr, obj_ptr}
    , ctrl_ptr_(ctrl_ptr) {};

    friend void delete_(alloc_unique_trait_ptr* v) {
        if (v->obj_ptr_ == nullptr)
            return;
        if (v->ctrl_ptr_ != nullptr)
            v->ctrl_ptr_->destructor_ptr_(v->ctrl_ptr_);
        else
            v->vtable_ptr_->default_delete(v->obj_ptr_);
    }

public:
    alloc_unique_trait_ptr() = default;
    alloc_unique_trait_ptr(unique_trait_ptr<Trait>&& other)    // NOLINT(*explicit*, *param-not-moved*)
    : impl_t{other.vtable_ptr_, other.obj_ptr_} {
        other.obj_ptr = nullptr;
    };

    alloc_unique_trait_ptr(alloc_unique_trait_ptr&& other) noexcept
    : impl_t{other.vtable_ptr_, other.obj_ptr_}
    , ctrl_ptr_{other.ctrl_ptr_} {
        // other.vtable_ptr_ = nullptr;
        other.obj_ptr_  = nullptr;
        other.ctrl_ptr_ = nullptr;
    }
    alloc_unique_trait_ptr& operator=(alloc_unique_trait_ptr&& other) noexcept {
        delete_(this);
        this->vtable_ptr_ = other.vtable_ptr_;
        this->obj_ptr_    = other.obj_ptr_;
        this->ctrl_ptr_   = other.ctrl_ptr_;
        // other.vtable_ptr_ = nullptr;
        other.obj_ptr_  = nullptr;
        other.ctrl_ptr_ = nullptr;
        return *this;
    };

    alloc_unique_trait_ptr(const alloc_unique_trait_ptr&)            = delete;
    alloc_unique_trait_ptr& operator=(const alloc_unique_trait_ptr&) = delete;

    ~alloc_unique_trait_ptr() {
        delete_(this);
    }
};

template<any_trait Trait, implements_trait<Trait> Impl, typename Alloc, typename... Args>
auto allocate_unique_trait(const Alloc& allocator, Args&&... args) {
    using alloc        = std::allocator_traits<Alloc>::template rebind_alloc<std::byte>;
    using ctrl_block   = detail::ctrl_block<Impl, alloc, detail::empty_0>;
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
        return alloc_unique_trait_ptr<Trait>(vtable_ptr, impl_ptr, cptr);
    } catch (...) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw;
    }
}
}    // namespace trp
