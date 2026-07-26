#pragma once
#ifndef TRP_GODBOLT
#include "detail/vtable.hpp"
#endif

namespace trp {
template<any_trait Trait>
struct dyn_trait_ref;

/**
 * @brief Converts reference from one trait to its explicit supertrait.
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

template<auto& MethodIds, auto& Idxs, uZ... Is>
struct cvm_invoker : cvo_invoker<Idxs[Is], typename[:MethodIds[Is]:]>... {
    static constexpr auto identifier = [:MethodIds[0]:] ::identifier;
    using cvo_invoker<Idxs[Is], typename[:MethodIds[Is]:]>::operator()...;
};

template<typename CVMInvoker, typename VTable, typename... Args>
concept valid_cvm_invoker = has_template_arguments(^^CVMInvoker)                //
                            and (template_of(^^CVMInvoker) == ^^cvm_invoker)    //
                            and std::invocable<CVMInvoker, VTable const*, void*, Args...>;

template<typename CVMInvoker, typename VTable, typename... Args>
inline constexpr auto noexcept_cvm_invoker =
    std::is_nothrow_invocable_v<CVMInvoker, VTable const*, void*, Args...>;


template<typename TRef, typename Trait, typename MethodHolder, typename CVMInvoker>
struct method_invoker {
private:
    using trait_t   = Trait;
    using vtable_t  = vtable<std::remove_cv_t<trait_t>>;
    using invoker_t = [:copy_cv_to(^^trait_t, ^^CVMInvoker):];

public:
    template<typename... Args>
    auto operator()(Args&&... args) const
        volatile noexcept(noexcept_cvm_invoker<invoker_t, vtable_t, Args...>) -> decltype(auto)
        requires valid_cvm_invoker<invoker_t, vtable_t, Args...>
    {
        // call through a shim to delegate cv overload resolution to language
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

template<typename Ref, typename Trait, typename CVMInvoker>
struct method_holder_definer {
    struct method_holder;
    consteval {
        constexpr auto method_invoker_info =
            substitute(^^method_invoker, {^^Ref, ^^Trait, ^^method_holder, ^^CVMInvoker});
        define_aggregate(^^method_holder,
                         {data_member_spec(method_invoker_info,
                                           {
                                               .name              = CVMInvoker::identifier,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};

template<any_trait Trait, typename... CVMInvokers>
class dyn_trait_ref_impl
: public method_holder_definer<dyn_trait_ref_impl<Trait, CVMInvokers...>, Trait, CVMInvokers>::
      method_holder... {
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

consteval auto get_dyn_ref_impls(meta::info trait) {
    auto ref_targs    = std::vector{trait};
    auto ref_targs_c  = std::vector{add_const(trait)};
    auto ref_targs_v  = std::vector{add_volatile(trait)};
    auto ref_targs_cv = std::vector{add_cv(trait)};

    auto cvm_idts    = std::vector<meta::info>{};
    auto cvm_idts_c  = std::vector<meta::info>{};
    auto cvm_idts_v  = std::vector<meta::info>{};
    auto cvm_idts_cv = std::vector<meta::info>{};

    auto cvm_idxs    = std::vector<uZ>{};
    auto cvm_idxs_c  = std::vector<uZ>{};
    auto cvm_idxs_v  = std::vector<uZ>{};
    auto cvm_idxs_cv = std::vector<uZ>{};

    auto const all_methods = subextract_info_span(^^all_trait_methods, {trait});
    auto const groups      = subextract_span<method_reference>(^^trait_method_groups, {trait});
    for (auto grp: groups) {
        auto const id = grp.name;
        for (auto const i: stdv::iota(grp.begin_idx, grp.end_idx)) {
            auto const mem   = all_methods[i];
            auto const quals = extract_method_qualifiers(mem);
            if (quals.is_rvalue)
                continue;

            cvm_idts.push_back(mem);
            cvm_idxs.push_back(i);
            if (quals.is_const) {
                cvm_idts_c.push_back(mem);
                cvm_idxs_c.push_back(i);
            }
            if (quals.is_volatile) {
                cvm_idts_v.push_back(mem);
                cvm_idxs_v.push_back(i);
            }
            if (quals.is_cv()) {
                cvm_idts_cv.push_back(mem);
                cvm_idxs_cv.push_back(i);
            }
        };

        if (not cvm_idts.empty()) {
            auto invoker_targs =
                std::vector{meta::reflect_constant_array(cvm_idts), meta::reflect_constant_array(cvm_idxs)};
            for (auto i: stdv::iota(0U, cvm_idxs.size()))
                invoker_targs.push_back(meta::reflect_constant(i));
            ref_targs.push_back(substitute(^^cvm_invoker, invoker_targs));
            cvm_idts.clear();
            cvm_idxs.clear();
        }
        if (not cvm_idts_c.empty()) {
            auto invoker_targs = std::vector{meta::reflect_constant_array(cvm_idts_c),
                                             meta::reflect_constant_array(cvm_idxs_c)};
            for (auto i: stdv::iota(0U, cvm_idxs_c.size()))
                invoker_targs.push_back(meta::reflect_constant(i));
            ref_targs_c.push_back(substitute(^^cvm_invoker, invoker_targs));
            cvm_idts_c.clear();
            cvm_idxs_c.clear();
        }
        if (not cvm_idts_v.empty()) {
            auto invoker_targs = std::vector{meta::reflect_constant_array(cvm_idts_v),
                                             meta::reflect_constant_array(cvm_idxs_v)};
            for (auto i: stdv::iota(0U, cvm_idxs_v.size()))
                invoker_targs.push_back(meta::reflect_constant(i));
            ref_targs_v.push_back(substitute(^^cvm_invoker, invoker_targs));
            cvm_idts_v.clear();
            cvm_idxs_v.clear();
        }
        if (not cvm_idts_cv.empty()) {
            auto invoker_targs = std::vector{meta::reflect_constant_array(cvm_idts_cv),
                                             meta::reflect_constant_array(cvm_idxs_cv)};
            for (auto i: stdv::iota(0U, cvm_idxs_cv.size()))
                invoker_targs.push_back(meta::reflect_constant(i));
            ref_targs_cv.push_back(substitute(^^cvm_invoker, invoker_targs));
            cvm_idts_cv.clear();
            cvm_idxs_cv.clear();
        }
    }

    return std::array{
        substitute(^^dyn_trait_ref_impl, ref_targs),
        substitute(^^dyn_trait_ref_impl, ref_targs_c),
        substitute(^^dyn_trait_ref_impl, ref_targs_v),
        substitute(^^dyn_trait_ref_impl, ref_targs_cv),
    };
}

template<non_cv_trait Trait>
inline constexpr auto dyn_ref_impls = get_dyn_ref_impls(^^Trait);

template<any_trait Trait>
using dyn_trait_ref_alias =    //
[:is_const(^^Trait) and is_volatile(^^Trait) ? dyn_ref_impls<std::remove_cv_t<Trait>>[3]
  : is_volatile(^^Trait)                     ? dyn_ref_impls<std::remove_cv_t<Trait>>[2]
  : is_const(^^Trait)                        ? dyn_ref_impls<std::remove_cv_t<Trait>>[1]
                                             : dyn_ref_impls<std::remove_cv_t<Trait>>[0]:];
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
