#pragma once
#ifndef TRP_GODBOLT
#include "default_trait_impl.hpp"
#endif

namespace trp::detail {

inline constexpr struct {
} vtable_ptr_anno;
inline constexpr struct {
} obj_ptr_anno;


template<non_cvref TRef>
inline constexpr auto vtable_member_info = find_annotated_member(^^TRef,    //
                                                                 vtable_ptr_anno,
                                                                 "No vtable annotated member found",
                                                                 meta::access_context::unchecked());

template<non_cvref TRef>
inline constexpr auto obj_member_info = find_annotated_member(^^TRef,    //
                                                              obj_ptr_anno,
                                                              "No object annotated member found",
                                                              meta::access_context::unchecked());

auto extract_vtable_ptr(auto&& ref) -> auto&&
    requires(vtable_member_info<std::remove_cvref_t<decltype(ref)>> != meta::info{})
{
    return ref.[:vtable_member_info<std::remove_cvref_t<decltype(ref)>>:];
}
auto extract_obj_ptr(auto&& ref) -> auto&&
    requires(obj_member_info<std::remove_cvref_t<decltype(ref)>> != meta::info{})
{
    return std::forward<decltype(ref)>(ref).[:obj_member_info<std::remove_cvref_t<decltype(ref)>>:];
}

namespace cvts_trait {


template<non_ref Impl, meta::info Method, bool ExplicitMethod, trait_method_idt MethodIdt>
struct cvts_overload_spec {
    using method_idt = MethodIdt;
};

// cv-transient static cv-overload invoker
template<non_cvref TRef, non_cvref MethodHolder, non_cvref MethodInvoker, non_cvref OvSpec>
struct cvts_cvo_invoker;


consteval auto get_first_member_check_type(meta::info holder, meta::info type) {
    auto const mems = nonstatic_data_members_of(holder, unprivileged);
    if (mems.size() != 1)
        throw "Method holder is expected to have only a single method.";
    if (type_of(mems[0]) != type)
        throw "Method invoker type does not match method holders first member type.";
    return mems[0];
}

#define TRP_DEV
#ifdef TRP_DEV

#define TRP_ASSERT_DERIVED_INVOKER static_assert(std::derived_from<MethodInvoker, cvts_cvo_invoker>);
#define TRP_ASSERT_STANDARD_LAYOUT static_assert(std::is_standard_layout_v<MethodHolder>);
#ifdef __cpp_lib_is_pointer_interconvertible
#define TRP_ASSERT_INTERCONVERTIBLE                                                                       \
    static_assert(                                                                                        \
        std::is_pointer_interconvertible_with_class<MethodHolder>(extract<MethodInvoker MethodHolder::*>( \
            get_first_member_check_type(^^MethodHolder, ^^MethodInvoker))));
#else
#define TRP_ASSERT_INTERCONVERTIBLE
#endif
#define TRP_ASSERT_DERIVED_TREF static_assert(std::derived_from<TRef, MethodHolder>);
#else

#define TRP_ASSERT_DERIVED_INVOKER
#define TRP_ASSERT_STANDARD_LAYOUT
#define TRP_ASSERT_INTERCONVERTIBLE
#define TRP_ASSERT_DERIVED_TREF

#endif
#define TRP_ADD_CVP(type) add_pointer(copy_cv_to(^^std::remove_pointer_t<decltype(this)>, type))

#define TRP_CV_OVERLOAD(Req, C, V, Ref)                                                                  \
    template<non_cvref           TRef,                                                                   \
             non_cvref           MethodHolder,                                                           \
             non_cvref           MethodInvoker,                                                          \
             non_ref             Impl,                                                                   \
             meta::info          Method,                                                                 \
             bool                ExplicitMethod,                                                         \
             char const*         Id,                                                                     \
             method_qualifiers_t Quals,                                                                  \
             typename Ret,                                                                               \
             typename... Args>                                                                           \
        requires(Req)                                                                                    \
    struct cvts_cvo_invoker<                                                                             \
        TRef,                                                                                            \
        MethodHolder,                                                                                    \
        MethodInvoker,                                                                                   \
        cvts_overload_spec<Impl, Method, ExplicitMethod, method_identity_t<Id, Quals, Ret, Args...>>> {  \
        auto operator()(Args... args) C V Ref noexcept(Quals.is_noexcept) -> Ret {                       \
            using self_t = [:Quals.is_rvalue ? (^^cvts_cvo_invoker C V&&) : (^^cvts_cvo_invoker C V&):]; \
            if constexpr (ExplicitMethod) {                                                              \
                return [:substitute(Method, {^^TRef}):](static_cast<self_t&&>(*this).get_trait_ref(),    \
                                                        std::forward<Args>(args)...);                    \
            } else {                                                                                     \
                using effective_ref_t = [:Quals.is_rvalue ? add_rvalue_reference(^^Impl)                 \
                                                          : add_lvalue_reference(^^Impl):];              \
                if constexpr (is_function_template(Method)) {                                            \
                    return static_cast<effective_ref_t>(                                                 \
                               *extract_obj_ptr(static_cast<self_t&&>(*this).get_trait_ref()))           \
                        .template[:Method:](std::forward<Args>(args)...);                                \
                } else {                                                                                 \
                    return static_cast<effective_ref_t>(                                                 \
                               *extract_obj_ptr(static_cast<self_t&&>(*this).get_trait_ref()))           \
                        .[:Method:](std::forward<Args>(args)...);                                        \
                }                                                                                        \
            }                                                                                            \
        }                                                                                                \
                                                                                                         \
    private:                                                                                             \
        auto get_trait_ref() C V Ref -> decltype(auto) {                                                 \
            TRP_ASSERT_DERIVED_INVOKER                                                                   \
            auto const mi_ptr = static_cast<[:TRP_ADD_CVP(^^MethodInvoker):]>(this);                     \
                                                                                                         \
            TRP_ASSERT_STANDARD_LAYOUT                                                                   \
            TRP_ASSERT_INTERCONVERTIBLE                                                                  \
            auto const mh_ptr = reinterpret_cast<[:TRP_ADD_CVP(^^MethodHolder):]>(mi_ptr);               \
                                                                                                         \
            TRP_ASSERT_DERIVED_TREF                                                                      \
                                                                                                         \
            using out_ref_t = [:Quals.is_rvalue ? (^^TRef C V&&) : (^^TRef C V&):];                      \
            return static_cast<out_ref_t>(*static_cast<[:TRP_ADD_CVP(^^TRef):]>(mh_ptr));                \
        }                                                                                                \
    };

// clang-format off

TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile and not Quals.is_ref(),       ,         ,   );
TRP_CV_OVERLOAD(    Quals.is_const and not Quals.is_volatile and not Quals.is_ref(),  const,         ,   );
TRP_CV_OVERLOAD(not Quals.is_const and     Quals.is_volatile and not Quals.is_ref(),       , volatile,   );
TRP_CV_OVERLOAD(    Quals.is_const and     Quals.is_volatile and not Quals.is_ref(),  const, volatile,   );

TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile and     Quals.is_lvalue,      ,         , & );
TRP_CV_OVERLOAD(    Quals.is_const and not Quals.is_volatile and     Quals.is_lvalue, const,         , & );
TRP_CV_OVERLOAD(not Quals.is_const and     Quals.is_volatile and     Quals.is_lvalue,      , volatile, & );
TRP_CV_OVERLOAD(    Quals.is_const and     Quals.is_volatile and     Quals.is_lvalue, const, volatile, & );

TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile and     Quals.is_rvalue,      ,         , &&);
TRP_CV_OVERLOAD(    Quals.is_const and not Quals.is_volatile and     Quals.is_rvalue, const,         , &&);
TRP_CV_OVERLOAD(not Quals.is_const and     Quals.is_volatile and     Quals.is_rvalue,      , volatile, &&);
TRP_CV_OVERLOAD(    Quals.is_const and     Quals.is_volatile and     Quals.is_rvalue, const, volatile, &&);

// clang-format on

#undef TRP_ASSERT_DERIVED_TREF
#undef TRP_ASSERT_INTERCONVERTIBLE
#undef TRP_ASSERT_STANDARD_LAYOUT
#undef TRP_ASSERT_DERIVED_INVOKER

#undef TRP_ADD_CVP
#undef TRP_CV_OVERLOAD
#undef TRP_ASSERT_INTERCONVERTIBLE

// cv-transient static cv-method invoker
template<typename TRef, typename MethodHolder, typename... OvSpecs>
struct cvts_cvm_invoker
: cvts_cvo_invoker<TRef, MethodHolder, cvts_cvm_invoker<TRef, MethodHolder, OvSpecs...>, OvSpecs>... {
    using cvts_cvo_invoker<TRef, MethodHolder, cvts_cvm_invoker, OvSpecs>::operator()...;
};

template<typename Ref, typename... OvSpecs>
struct cvts_holder_definer {
    struct cvts_method_holder;
    consteval {
        using invoker_t = cvts_cvm_invoker<Ref, cvts_method_holder, OvSpecs...>;
        define_aggregate(^^cvts_method_holder,
                         {data_member_spec(^^invoker_t,
                                           {
                                               .name              = OvSpecs...[0] ::method_idt::identifier,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};

consteval auto get_holder_definer(meta::info ref, meta::info ov_specs_reflection) -> meta::info {
    auto       targs = std::vector{ref};
    auto const ov_specs =
        std::span(extract<meta::info const*>(ov_specs_reflection), extent(type_of(ov_specs_reflection)));
    targs.append_range(ov_specs);
    return substitute(^^cvts_holder_definer, targs);
}

template<typename Trait, typename Impl, meta::info... OvSpecsRefls>
class cvts_trait_ref_impl
: public[:get_holder_definer(^^cvts_trait_ref_impl<Trait, Impl, OvSpecsRefls...>,
                             OvSpecsRefls):] ::cvts_method_holder... {
    [[= obj_ptr_anno]] Impl* _;

    template<non_cvref, non_cvref, non_cvref, non_cvref>
    friend struct cvts_cvo_invoker;

public:
    explicit cvts_trait_ref_impl(Impl & optr) noexcept {
        extract_obj_ptr(*this) = &optr;
    };
    cvts_trait_ref_impl(cvts_trait_ref_impl const&) = delete;
    cvts_trait_ref_impl(cvts_trait_ref_impl&&)      = delete;

    template<typename T, typename S>
        requires std::constructible_from<T&, typename[:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):]>
    explicit operator T&(this S && self) noexcept {
        using source_t = [:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):];
        return static_cast<source_t>(*extract_obj_ptr(std::forward<S>(self)));
    }
    template<typename T, typename S>
        requires std::constructible_from<T&&, typename[:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):]>
    explicit operator T&&(this S && self) noexcept {
        using source_t = [:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):];
        return static_cast<source_t>(*extract_obj_ptr(std::forward<S>(self)));
    }
};

consteval auto get_ref_impls(meta::info trait, meta::info impl) {
    auto ref_targs    = std::vector{trait, impl};
    auto ref_targs_c  = std::vector{add_const(trait), impl};
    auto ref_targs_v  = std::vector{add_volatile(trait), impl};
    auto ref_targs_cv = std::vector{add_cv(trait), impl};

    auto methods    = std::vector<meta::info>{};
    auto methods_c  = std::vector<meta::info>{};
    auto methods_v  = std::vector<meta::info>{};
    auto methods_cv = std::vector<meta::info>{};

    auto const all_methods = subextract_span<meta::info>(^^all_trait_methods, {trait});
    auto const all_impls   = subextract_span<impl_method_bind>(^^full_impls_for, {impl, trait});
    auto const groups      = subextract_span<method_reference>(^^trait_method_groups, {trait});
    for (auto const grp: groups) {
        for (auto i: stdv::iota(grp.begin_idx, grp.end_idx)) {
            auto const mem    = all_methods[i];
            auto const impl_m = all_impls[i];
            auto const quals  = extract_method_qualifiers(mem);
            auto const spec   = substitute(
                ^^cvts_overload_spec,
                {impl, meta::reflect_constant(impl_m.fn), meta::reflect_constant(impl_m.is_explicit), mem});
            methods.push_back(spec);
            if (quals.is_const)
                methods_c.push_back(spec);
            if (quals.is_volatile)
                methods_v.push_back(spec);
            if (quals.is_const and quals.is_volatile)
                methods_cv.push_back(spec);
        };

        if (not methods.empty()) {
            ref_targs.push_back(reflect_constant(meta::reflect_constant_array(methods)));
            methods.clear();
        }
        if (not methods_c.empty()) {
            ref_targs_c.push_back(reflect_constant(meta::reflect_constant_array(methods_c)));
            methods_c.clear();
        }
        if (not methods_v.empty()) {
            ref_targs_v.push_back(reflect_constant(meta::reflect_constant_array(methods_v)));
            methods_v.clear();
        }
        if (not methods_cv.empty()) {
            ref_targs_cv.push_back(reflect_constant(meta::reflect_constant_array(methods_cv)));
            methods_cv.clear();
        }
    }
    return std::array{
        substitute(^^cvts_trait_ref_impl, ref_targs),
        substitute(^^cvts_trait_ref_impl, ref_targs_c),
        substitute(^^cvts_trait_ref_impl, ref_targs_v),
        substitute(^^cvts_trait_ref_impl, ref_targs_cv),
    };
}

template<non_cvref Trait, non_ref Impl>
inline constexpr auto ref_impls = get_ref_impls(^^Trait, ^^Impl);

template<any_trait Trait, implements_trait<Trait> Impl>
using cvts_trait_ref =    //
[:is_const(^^Trait) and is_volatile(^^Trait) ? ref_impls<std::remove_cv_t<Trait>, Impl>[3]
  : is_volatile(^^Trait)                     ? ref_impls<std::remove_cv_t<Trait>, Impl>[2]
  : is_const(^^Trait)                        ? ref_impls<std::remove_cv_t<Trait>, Impl>[1]
                                             : ref_impls<std::remove_cv_t<Trait>, Impl>[0]:];

struct name_info_pair {
    char const* name;
    meta::info  member;
};

consteval auto get_all_cvref_methods(meta::info cvts_ref) -> std::span<meta::info const> {
    auto mems       = std::vector<meta::info>{};
    auto types      = std::vector<meta::info>{cvts_ref};
    auto next_types = std::vector<meta::info>{};
    while (not types.empty()) {
        for (auto t: types) {
            for (auto m: nonstatic_data_members_of(t, unprivileged))
                mems.push_back(m);
            next_types.append_range(subextract_base_types(t));
        }
        if (not mems.empty())
            break;    // all members appear on one level of inheritance
        swap(types, next_types);
        next_types.clear();
    }
    return std::define_static_array(mems);
}

template<non_cvref CvtsRef>
inline constexpr auto all_cvref_methods = get_all_cvref_methods(^^CvtsRef);

consteval auto get_ref_method(std::string_view id, meta::info cvts_ref) {
    auto const mems = subextract_info_span(^^all_cvref_methods, {cvts_ref});
    for (auto m: mems) {
        if (identifier_of(m) == id)
            return m;
    }
    std::unreachable();
};

template<char const* MethodId, typename Ref, typename... Args>
auto call_method_via_id(Ref&& ref, Args&&... args) -> decltype(auto) {
    constexpr auto ref_method = get_ref_method(MethodId, remove_cvref(^^Ref));
    using fn_t = [:is_rvalue_reference_type(^^Ref&&) ? add_rvalue_reference(type_of(ref_method))
                                                     : add_lvalue_reference(type_of(ref_method)):];

    return static_cast<fn_t>(ref.[:ref_method:])(std::forward<Args>(args)...);
}
}    // namespace cvts_trait


// cv-transient static trait reference
template<any_trait Trait, implements_trait<Trait> Impl>
using cvts_trait_ref = cvts_trait::cvts_trait_ref<Trait, Impl>;
}    // namespace trp::detail
