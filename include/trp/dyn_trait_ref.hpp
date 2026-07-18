#pragma once
#ifndef TRP_GODBOLT
#include "detail/vtable.hpp"
#endif

namespace trp {
template<any_trait Trait>
struct dyn_trait_ref;

/**
 * @brief Converts reference from one trait to it's explicit supertrait. 
 * The resulting vtable uses entries from vtable of `Impl` for `Supertrait`.
 *
 * @tparam Supertrait target trait.
 */
template<any_trait Supertrait, typename Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
[[nodiscard]] auto trait_cast(dyn_trait_ref<Trait> const& ref) noexcept -> dyn_trait_ref<Supertrait> {
    return ref;
}
/**
 * @brief Converts reference from one trait to other trait. 
 * The resulting vtable always matches vtable of `Impl` for `Trait`,
 * even if `Trait` is an explicit supertrait of `U`.
 * This differs from `trait_cast` without explicit `Impl` parameter.
 * If the underlying object's type is not `Impl`, the behavior is undefined.
 *
 * @tparam Trait target trait.
 * @tparam Impl  referenced value's type.
 */
template<any_trait Trait, implements_trait<Trait> Impl, typename U>
[[nodiscard]] auto trait_cast(dyn_trait_ref<U> const& ref) -> dyn_trait_ref<Trait> {
    return {&detail::trait_vtable_for<std::remove_cv_t<Trait>, std::remove_cv_t<Trait>, Impl>,
            detail::extract_obj_ptr(ref)};
}
/**
 * @brief Checks if the referenced value's type is Impl.
 */
template<typename Impl, any_trait Trait>
    requires implements_trait<Impl, Trait>
[[nodiscard]] auto is_holding_type(dyn_trait_ref<Trait> const& ref) noexcept -> bool {
    return detail::extract_obj_ptr(ref)    //
           and detail::extract_vtable_ptr(ref)->id_ptr == &detail::unique_id_struct<Impl>::value;
}
namespace detail {
template<typename T>
concept any_dyn_trait_ref = has_template_arguments(^^T) and template_of(^^T) == ^^dyn_trait_ref;

template<uZ Index, trait_method_idt MethodId>
struct cvo_invoker;

#define TRP_CV_OVERLOAD(Req, C, V)                                                                           \
    template<uZ Index, auto Identifier, method_qualifiers_t Quals, typename Ret, typename... Args>           \
        requires(Req)                                                                                        \
    struct cvo_invoker<Index, method_identity_t<Identifier, Quals, Ret, Args...>> {                          \
        auto operator()(auto const* vtable_ptr, void* obj_ptr, Args... args) C V noexcept(Quals.is_noexcept) \
            -> Ret {                                                                                         \
            return get_method(vtable_ptr)(obj_ptr, std::forward<Args>(args)...);                             \
        }                                                                                                    \
                                                                                                             \
    private:                                                                                                 \
        template<typename VTable>                                                                            \
        static auto get_method(VTable* vt) {                                                                 \
            constexpr auto m =                                                                               \
                nonstatic_data_members_of(^^typename VTable::vtable_impl, unprivileged)[Index];              \
            return vt->[:m:];                                                                                \
        }                                                                                                    \
    };

// clang-format off
TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile,      ,         );
TRP_CV_OVERLOAD(    Quals.is_const and not Quals.is_volatile, const,         );
TRP_CV_OVERLOAD(not Quals.is_const and     Quals.is_volatile,      , volatile);
TRP_CV_OVERLOAD(    Quals.is_const and     Quals.is_volatile, const, volatile);
// clang-format on
#undef TRP_CV_OVERLOAD


template<uZ I, trait_method_idt MethodId>
struct overload_spec {
    using id                  = MethodId;
    static constexpr uZ index = I;
};
template<typename... OvSpecs>
struct cvm_invoker : cvo_invoker<OvSpecs::index, typename OvSpecs::id>... {
    using cvo_invoker<OvSpecs::index, typename OvSpecs::id>::operator()...;
};

template<typename CVMInvoker, typename VTable, typename... Args>
concept noexcept_cvm_invoker =
    has_template_arguments(^^CVMInvoker)                //
    and (template_of(^^CVMInvoker) == ^^cvm_invoker)    //
    and ([] {
            VTable vt{};
            return noexcept(CVMInvoker{}(&vt, nullptr, std::forward<Args>(std::declval<Args>())...));
        }());


template<typename TRef, typename Trait, typename MethodHolder, typename CVMInvoker>
struct method_invoker {
private:
    using trait_t   = Trait;
    using vtable_t  = vtable<std::remove_cv_t<trait_t>>;
    using invoker_t = [:copy_cv_to(^^trait_t, ^^CVMInvoker):];

public:
    template<typename... Args>
    auto operator()(Args&&... args) const
        volatile noexcept(noexcept_cvm_invoker<invoker_t, vtable_t, Args...>) -> decltype(auto) {
        return invoker_t{}(extract_vtable_ptr(get_trait_ref()),
                           extract_obj_ptr(get_trait_ref()),
                           std::forward<Args>(args)...);
    }

private:
    auto get_trait_ref(this auto&& self) -> decltype(auto) {
        constexpr auto add_cvp = [](meta::info type) {
            using this_t = std::remove_reference_t<decltype(self)>;
            return add_pointer(copy_cv_to(^^this_t, type));
        };

        constexpr auto invoker_ptr = [] {
            auto mems = nonstatic_data_members_of(^^MethodHolder, unprivileged);
            if (mems.size() != 1)
                throw "Method holder is expected to have only a single method.";
            if (type_of(mems[0]) != ^^method_invoker)
                throw "Method invoker type does not match method holders first member type.";
            return extract<method_invoker MethodHolder::*>(mems[0]);
        }();
#ifdef __cpp_lib_is_pointer_interconvertible
        static_assert(std::is_pointer_interconvertible_with_class<MethodHolder>(invoker_ptr));
#endif
        static_assert(std::is_standard_layout_v<MethodHolder>);
        auto const mh_ptr = reinterpret_cast<[:add_cvp(^^MethodHolder):]>(&self);

        static_assert(std::derived_from<TRef, MethodHolder>);
        return *static_cast<[:add_cvp(^^TRef):]>(mh_ptr);
    }
};

template<char const* Id, typename Ref, typename Trait, typename CVMInvoker>
struct method_holder_definer {
    struct method_holder;
    consteval {
        constexpr auto method_invoker_info =
            substitute(^^method_invoker, {^^Ref, ^^Trait, ^^method_holder, ^^CVMInvoker});
        define_aggregate(^^method_holder,
                         {data_member_spec(method_invoker_info,
                                           {
                                               .name              = Id,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};
template<char const* Id, typename Ref, typename Trait, typename CVMInvoker>
inline constexpr auto method_holder_info =
    ^^typename method_holder_definer<Id, Ref, Trait, CVMInvoker>::method_holder;

template<any_trait Trait, typename... MethodHolders>
class dyn_trait_ref_impl : public MethodHolders... {
    [[= vtable_ptr_anno]] vtable<std::remove_cv_t<Trait>> const* _{};
    [[= obj_ptr_anno]] void*                                     _;

protected:
    dyn_trait_ref_impl() {
        extract_obj_ptr(*this) = nullptr;
    };
    dyn_trait_ref_impl(vtable<std::remove_cv_t<Trait>> const* vptr, auto* optr) {
        extract_vtable_ptr(*this) = vptr;
        extract_obj_ptr(*this)    = (void*)optr;
    };

    template<any_trait U, typename... MHs>
    friend void ref_release(dyn_trait_ref_impl<U, MHs...>& ref);
    template<any_trait U, typename... MHs>
    friend void ref_rebind(dyn_trait_ref_impl<U, MHs...>& ref, dyn_trait_ref_impl<U, MHs...> const& other);
    template<any_trait U, typename... MHs>
    friend auto ref_holds_value(dyn_trait_ref_impl<U, MHs...> const& ref) -> bool;
    template<any_trait U, typename... MHs>
    friend void ref_default_delete(dyn_trait_ref_impl<U, MHs...> const& ref);

    template<typename, typename, typename, typename>
    friend struct method_invoker;

    template<any_trait>
    friend class ::trp::dyn_trait_ref;
};

template<any_trait Trait, typename... MethodHolders>
void ref_release(dyn_trait_ref_impl<Trait, MethodHolders...>& ref) {
    extract_obj_ptr(ref) = nullptr;
}
template<any_trait Trait, typename... MethodHolders>
void ref_rebind(dyn_trait_ref_impl<Trait, MethodHolders...>&       ref,
                dyn_trait_ref_impl<Trait, MethodHolders...> const& other) {
    extract_vtable_ptr(ref) = extract_vtable_ptr(other);
    extract_obj_ptr(ref)    = extract_obj_ptr(other);
}
template<any_trait Trait, typename... MethodHolders>
[[nodiscard]] auto ref_holds_value(dyn_trait_ref_impl<Trait, MethodHolders...> const& ref) -> bool {
    return extract_obj_ptr(ref) != nullptr;
}
template<any_trait Trait, typename... MethodHolders>
void ref_default_delete(dyn_trait_ref_impl<Trait, MethodHolders...> const& ref) {
    extract_vtable_ptr(ref)->default_delete(extract_obj_ptr(ref));
}
template<non_cv_trait Trait>
struct dyn_cv_ref_definer {
    struct ref;
    struct ref_c;
    struct ref_v;
    struct ref_cv;

    static constexpr auto impls = [] {
        auto ref_targs    = std::vector{^^Trait};
        auto ref_targs_c  = std::vector{add_const(^^Trait)};
        auto ref_targs_v  = std::vector{add_volatile(^^Trait)};
        auto ref_targs_cv = std::vector{add_cv(^^Trait)};

        auto holder_targs    = std::vector<meta::info>{};
        auto holder_targs_c  = std::vector<meta::info>{};
        auto holder_targs_v  = std::vector<meta::info>{};
        auto holder_targs_cv = std::vector<meta::info>{};

        for (auto grp: trait_method_groups<Trait>) {
            auto const id          = grp.name;
            auto const raw_methods = all_trait_methods<Trait>     //
                                     | stdv::take(grp.end_idx)    //
                                     | stdv::drop(grp.begin_idx);
            for (auto [i, mem]: stdv::zip(stdv::iota(grp.begin_idx), raw_methods)) {
                auto const quals     = extract_method_qualifiers(mem);
                auto const spec      = substitute(^^overload_spec, {meta::reflect_constant(i), mem});
                auto const maybe_add = [=](auto& targs, bool do_add) {
                    if (not do_add)
                        return;
                    targs.push_back(spec);
                };

                maybe_add(holder_targs, not quals.is_rvalue);
                maybe_add(holder_targs_c, quals.is_const and not quals.is_rvalue);
                maybe_add(holder_targs_v, quals.is_volatile and not quals.is_rvalue);
                maybe_add(holder_targs_cv, quals.is_cv() and not quals.is_rvalue);
            };

            auto const id_refl      = meta::reflect_constant(id);
            auto const add_nonempty = [=](auto& targs, auto ref, auto trait, auto& holder_targs) {
                if (holder_targs.empty())
                    return;
                auto const invoker = substitute(^^cvm_invoker, holder_targs);
                targs.push_back(
                    extract<meta::info>(substitute(^^method_holder_info, {id_refl, ref, trait, invoker})));
                holder_targs.clear();
            };
            add_nonempty(ref_targs, ^^ref, ^^Trait, holder_targs);
            add_nonempty(ref_targs_c, ^^ref_c, add_const(^^Trait), holder_targs_c);
            add_nonempty(ref_targs_v, ^^ref_v, add_volatile(^^Trait), holder_targs_v);
            add_nonempty(ref_targs_cv, ^^ref_cv, add_cv(^^Trait), holder_targs_cv);
        }

        return std::array{
            substitute(^^dyn_trait_ref_impl, ref_targs),
            substitute(^^dyn_trait_ref_impl, ref_targs_c),
            substitute(^^dyn_trait_ref_impl, ref_targs_v),
            substitute(^^dyn_trait_ref_impl, ref_targs_cv),
        };
    }();
    using impl_t    = [:impls[0]:];
    using impl_c_t  = [:impls[1]:];
    using impl_v_t  = [:impls[2]:];
    using impl_cv_t = [:impls[3]:];

    struct ref : public impl_t {
        using impl_t::impl_t;
    };
    struct ref_c : public impl_c_t {
        using impl_c_t::impl_c_t;
    };
    struct ref_v : public impl_v_t {
        using impl_v_t::impl_v_t;
    };
    struct ref_cv : public impl_cv_t {
        using impl_cv_t::impl_cv_t;
    };
};
template<any_trait Trait>
using dyn_trait_ref_alias = [:[] {
    using impls = dyn_cv_ref_definer<std::remove_cv_t<Trait>>;
    if constexpr (std::is_const_v<Trait> and std::is_volatile_v<Trait>) {
        return ^^typename impls::ref_cv;
    } else if constexpr (std::is_volatile_v<Trait>) {
        return ^^typename impls::ref_v;
    } else if constexpr (std::is_const_v<Trait>) {
        return ^^typename impls::ref_c;
    } else {
        return ^^typename impls::ref;
    }
}():];
}    // namespace detail

template<any_trait Trait>
class dyn_trait_ref : public detail::dyn_trait_ref_alias<Trait> {
    dyn_trait_ref() = default;

    template<any_trait>
    friend class shared_trait_ptr;
    template<any_trait>
    friend class unique_trait_ptr;
    template<any_trait>
    friend class alloc_unique_trait_ptr;
    template<any_trait>
    friend class dyn_trait_ref;

    template<any_trait U>
        requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<Trait>>
    [[nodiscard]] friend constexpr auto is_valid_const_trait_cast(dyn_trait_ref const& ref) -> bool {
        if constexpr (supertrait_of<U, Trait>) {
            return true;
        } else {
            if constexpr (non_cv_trait<U>) {
                return detail::extract_vtable_ptr(ref)->cv_quals.has_full;
            } else if constexpr (std::is_const_v<U>) {
                return detail::extract_vtable_ptr(ref)->cv_quals.has_const;
            } else if constexpr (std::is_volatile_v<U>) {
                return detail::extract_vtable_ptr(ref)->cv_quals.has_volatile;
            } else {
                // otherwise U has to be a supertrait of Trait
                static_assert(false);
            }
        }
    }
    template<any_trait U>
        requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<Trait>>
    [[nodiscard]] friend constexpr auto const_trait_cast(dyn_trait_ref const& ref) -> dyn_trait_ref<U> {
        return {detail::extract_vtable_ptr(ref), detail::extract_obj_ptr(ref)};
    }

    dyn_trait_ref(detail::vtable<std::remove_cv_t<Trait>> const* vptr, void* optr)
    : detail::dyn_trait_ref_alias<Trait>(vptr, optr) {};

    template<implements_trait<Trait> Impl>
    explicit dyn_trait_ref(Impl* obj)
    : detail::dyn_trait_ref_alias<Trait>(
          &detail::trait_vtable_for<std::remove_cv_t<Trait>, std::remove_cv_t<Trait>, Impl>, obj){};

    template<any_trait T, implements_trait<T> Impl, typename U>
    friend auto trait_cast(dyn_trait_ref<U> const& ref) -> dyn_trait_ref<T>;

public:
    template<any_trait U>
        requires explicit_supertrait_of<Trait, U> and (not std::same_as<Trait, U>)
    dyn_trait_ref(dyn_trait_ref<U> const& ref)    // NOLINT(*-explicit-*)
    : detail::dyn_trait_ref_alias<Trait>(
          get_explicit_supertrait_vtable_ptr<std::remove_cv_t<Trait>>(detail::extract_vtable_ptr(ref)),    //
          detail::extract_obj_ptr(ref)){};

    template<implements_trait<Trait> Impl>
        requires(not detail::any_dyn_trait_ref<Impl>)
    explicit dyn_trait_ref(Impl& obj)
    : detail::dyn_trait_ref_alias<Trait>(
          &detail::trait_vtable_for<std::remove_cv_t<Trait>, std::remove_cv_t<Trait>, Impl>, &obj){};

    dyn_trait_ref(dyn_trait_ref const&)            = default;
    dyn_trait_ref(dyn_trait_ref&&)                 = default;
    dyn_trait_ref& operator=(dyn_trait_ref const&) = delete;
    dyn_trait_ref& operator=(dyn_trait_ref&&)      = delete;
    ~dyn_trait_ref()                               = default;
};

template<any_trait U, any_trait T>
    requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<T>>
[[nodiscard]] static constexpr auto is_valid_const_trait_cast(dyn_trait_ref<T> ref) -> bool {
    return is_valid_const_trait_cast<U>(ref);
}
template<any_trait U, any_trait T>
    requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<T>>
[[nodiscard]] static constexpr auto const_trait_cast(dyn_trait_ref<T> ref) {
    return const_trait_cast<U>(ref);
}
}    // namespace trp
