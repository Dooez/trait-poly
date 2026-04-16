#pragma once
#ifndef TRP_GODBOLT
#include "allocator_ctrl_block.hpp"
#include "dyn_trait_ref.hpp"
#endif

#include <memory>
#include <stdexcept>

namespace trp {
template<any_trait Trait>
class alloc_unique_trait_ptr {
    using ctrl_header = detail::ctrl_header<>;
    mutable dyn_trait_ref<Trait> dyn_trait_ref_{};
    ctrl_header*                 ctrl_ptr_{};

    [[nodiscard]] auto get_ctrl_ptr() const -> ctrl_header* {
        return ctrl_ptr_;
    }

    template<implements_trait<Trait> Impl>
    alloc_unique_trait_ptr(Impl* obj_ptr, ctrl_header* ctrl_ptr)
    : dyn_trait_ref_(*obj_ptr)
    , ctrl_ptr_(ctrl_ptr){};

    explicit alloc_unique_trait_ptr(dyn_trait_ref<Trait> dyn_trait_ref, ctrl_header* ctrl_ptr = nullptr)
    : dyn_trait_ref_(dyn_trait_ref)
    , ctrl_ptr_(ctrl_ptr) {}

    void delete_() {
        if (not holds_value())
            return;
        if (ctrl_ptr_ != nullptr)
            ctrl_ptr_->destructor_ptr_(ctrl_ptr_);
        else
            detail::ref_default_delete(dyn_trait_ref_);
        release();
    }

    [[nodiscard]] bool holds_value() const {
        return detail::ref_holds_value(dyn_trait_ref_);
    }

    void release() {
        detail::ref_release(dyn_trait_ref_);
        ctrl_ptr_ = nullptr;
    }

public:
    alloc_unique_trait_ptr() = default;
    alloc_unique_trait_ptr(alloc_unique_trait_ptr&& other) noexcept
    : dyn_trait_ref_{other.dyn_trait_ref_}
    , ctrl_ptr_{other.ctrl_ptr_} {
        other.release();
    }
    alloc_unique_trait_ptr& operator=(alloc_unique_trait_ptr&& other) noexcept {
        if (this == &other)
            return *this;
        delete_();
        detail::ref_rebind(this->dyn_trait_ref_, other.dyn_trait_ref_);
        this->ctrl_ptr_ = other.ctrl_ptr_;
        other.release();
        return *this;
    };

    alloc_unique_trait_ptr(const alloc_unique_trait_ptr&)            = delete;
    alloc_unique_trait_ptr& operator=(const alloc_unique_trait_ptr&) = delete;

    ~alloc_unique_trait_ptr() {
        delete_();
    }

    explicit operator bool() const {
        return holds_value();
    }
    // UB if not holding value
    auto operator->(this auto&& self) -> decltype(auto) {
        return &self.dyn_trait_ref_;
    }
    // UB if not holding value
    auto operator*(this auto&& self) -> decltype(auto) {
        return self.dyn_trait_ref_;
    }

private:
    template<any_trait>
    friend class alloc_unique_trait_ptr;

    template<any_trait>
    friend class unique_trait_ptr;    // for conversion

    template<any_trait T, implements_trait<T>, typename Alloc, typename... Args>
    friend auto allocate_unique_trait(const Alloc&, Args&&...);

    template<typename S, typename T>
        requires explicit_supertrait_of<S, T>
    friend auto trait_cast(alloc_unique_trait_ptr<T>&& ptr);

    template<explicit_supertrait_of<Trait> Supertrait>
    [[nodiscard]] friend auto upcast(alloc_unique_trait_ptr ptr) -> alloc_unique_trait_ptr<Supertrait> {
        if (not ptr)
            return {};
        auto new_ptr = alloc_unique_trait_ptr<Supertrait>(trait_cast<Supertrait>(ptr.dyn_trait_ref_),
                                                          ptr.get_ctrl_ptr());
        ptr.release();
        return new_ptr;
    }
};

template<typename Supertrait, typename Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
[[nodiscard]] auto trait_cast(alloc_unique_trait_ptr<Trait> ptr) -> alloc_unique_trait_ptr<Supertrait> {
    return upcast<Supertrait>(std::move(ptr));
}

template<typename Impl, any_trait Trait>
    requires implements_trait<Impl, Trait>
[[nodiscard]] auto is_holding_type(const alloc_unique_trait_ptr<Trait>& ptr) -> bool {
    return ptr and is_holding_type<Impl>(*ptr);
}

template<any_trait Trait, implements_trait<Trait> Impl, typename Alloc, typename... Args>
[[nodiscard]] auto allocate_unique_trait(const Alloc& allocator, Args&&... args) {
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
        const auto impl_ptr = &(cptr->impl_);
        return alloc_unique_trait_ptr<Trait>(impl_ptr, cptr);
    } catch (...) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw;
    }
}

template<any_trait Trait>
class unique_trait_ptr {
    mutable dyn_trait_ref<Trait> dyn_trait_ref_;

    void release() {
        detail::ref_release(dyn_trait_ref_);
    }

    [[nodiscard]] bool holds_value() const {
        return detail::ref_holds_value(dyn_trait_ref_);
    }

    explicit unique_trait_ptr(dyn_trait_ref<Trait> trait_ref)
    : dyn_trait_ref_(trait_ref) {};

public:
    unique_trait_ptr() = default;
    unique_trait_ptr(unique_trait_ptr&& other) noexcept
    : dyn_trait_ref_{other.dyn_trait_ref_} {
        other.release();
    }
    unique_trait_ptr& operator=(unique_trait_ptr&& other) noexcept {
        if (this == &other)
            return *this;
        if (holds_value())
            detail::ref_default_delete(dyn_trait_ref_);
        detail::ref_rebind(dyn_trait_ref_, other.dyn_trait_ref_);
        other.release();
        return *this;
    };

    unique_trait_ptr(const unique_trait_ptr&)            = delete;
    unique_trait_ptr& operator=(const unique_trait_ptr&) = delete;

    ~unique_trait_ptr() {
        if (not holds_value())
            return;
        detail::ref_default_delete(dyn_trait_ref_);
        release();
    }
    explicit operator bool() const {
        return holds_value();
    }
    // UB if not holding value
    auto operator->(this auto&& self) -> dyn_trait_ref<Trait>* {
        return &self.dyn_trait_ref_;
    }
    // UB if not holding value
    auto operator*(this auto&& self) -> dyn_trait_ref<Trait>& {
        return self.dyn_trait_ref_;
    }

private:
    template<typename S, typename T>
        requires explicit_supertrait_of<S, T>
    friend auto trait_cast(unique_trait_ptr<T>&& ptr);

    template<explicit_supertrait_of<Trait> Supertrait>
    [[nodiscard]] friend auto upcast(unique_trait_ptr ptr) -> unique_trait_ptr<Supertrait> {
        if (not ptr)
            return {};
        auto new_ptr = unique_trait_ptr<Supertrait>(trait_cast<Supertrait>(ptr.dyn_trait_ref_));
        ptr.release();
        return new_ptr;
    }

public:
    // new allocated object only
    template<implements_trait<Trait> Impl>
    explicit unique_trait_ptr(Impl* obj_ptr)
    : dyn_trait_ref_(obj_ptr){};

    operator alloc_unique_trait_ptr<Trait>() && {    // NOLINT(*explicit*)
        auto new_ptr = alloc_unique_trait_ptr<Trait>(dyn_trait_ref_);
        release();
        return new_ptr;
    }
};

template<typename Supertrait, typename Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
[[nodiscard]] auto trait_cast(unique_trait_ptr<Trait> ptr) -> unique_trait_ptr<Supertrait> {
    return upcast<Supertrait>(std::move(ptr));
}
template<typename Impl, any_trait Trait>
    requires implements_trait<Impl, Trait>
[[nodiscard]] auto is_holding_type(const unique_trait_ptr<Trait>& ptr) -> bool {
    return ptr and is_holding_type<Impl>(*ptr);
}

template<any_trait Trait, implements_trait<Trait> Impl, typename... Args>
[[nodiscard]] auto make_unique_trait(Args&&... args) -> unique_trait_ptr<Trait> {
    return unique_trait_ptr<Trait>(new Impl(std::forward<Args>(args)...));
}
}    // namespace trp
