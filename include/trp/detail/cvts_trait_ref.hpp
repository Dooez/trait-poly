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
inline constexpr auto vtable_member_info = [] {
    auto m = find_annotated_member(^^TRef,    //
                                   vtable_ptr_anno,
                                   meta::access_context::unchecked());
    if (m == meta::info{})
        throw "No vtable annotated member found";
    return m;
}();

template<non_cvref TRef>
inline constexpr auto obj_member_info = [] {
    auto m = find_annotated_member(^^TRef,    //
                                   obj_ptr_anno,
                                   meta::access_context::unchecked());
    if (m == meta::info{})
        throw "No object annotated member found";
    return m;
}();

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

template<typename Trait, typename Impl, typename... MethodHolders>
class cvts_trait_ref_impl : public MethodHolders... {
    [[= obj_ptr_anno]] Impl* _;

    template<non_cvref, non_cvref, non_cvref, non_cvref>
    friend struct cvts_cvo_invoker;

public:
    explicit cvts_trait_ref_impl(Impl& optr) noexcept {
        extract_obj_ptr(*this) = &optr;
    };
    cvts_trait_ref_impl(cvts_trait_ref_impl const&) = delete;
    cvts_trait_ref_impl(cvts_trait_ref_impl&&)      = delete;

    template<typename T, typename S>
        requires std::constructible_from<T&, typename[:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):]>
    explicit operator T&(this S&& self) noexcept {
        using source_t = [:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):];
        return static_cast<source_t>(*extract_obj_ptr(std::forward<S>(self)));
    }
    template<typename T, typename S>
        requires std::constructible_from<T&&, typename[:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):]>
    explicit operator T&&(this S&& self) noexcept {
        using source_t = [:copy_cvref_to(^^S&&, copy_cv_to(^^Trait, ^^Impl)):];
        return static_cast<source_t>(*extract_obj_ptr(std::forward<S>(self)));
    }
};


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
            static_assert(std::is_standard_layout_v<MethodHolder>);                                      \
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

template<typename Ref, typename... OvSpecs>
using cvts_holder = typename cvts_holder_definer<Ref, OvSpecs...>::cvts_method_holder;

template<non_cvref Trait, non_ref Impl>
struct cvts_ref_definer {
    struct ref;
    struct ref_c;
    struct ref_v;
    struct ref_cv;

    static constexpr auto ref_impls = [] {
        auto ref_targs    = std::vector{^^Trait, ^^Impl};
        auto ref_targs_c  = std::vector{add_const(^^Trait), ^^Impl};
        auto ref_targs_v  = std::vector{add_volatile(^^Trait), ^^Impl};
        auto ref_targs_cv = std::vector{add_cv(^^Trait), ^^Impl};

        auto methods    = std::vector<meta::info>{^^ref};
        auto methods_c  = std::vector<meta::info>{^^ref_c};
        auto methods_v  = std::vector<meta::info>{^^ref_v};
        auto methods_cv = std::vector<meta::info>{^^ref_cv};
        for (auto const grp: trait_method_groups<Trait>) {
            auto const raw_methods = all_trait_methods<Trait>     //
                                     | stdv::take(grp.end_idx)    //
                                     | stdv::drop(grp.begin_idx);
            auto const raw_impls = full_impls_for<Impl, Trait>    //
                                   | stdv::take(grp.end_idx)      //
                                   | stdv::drop(grp.begin_idx);
            for (auto [i, mem, impl_m]: stdv::zip(stdv::iota(grp.begin_idx), raw_methods, raw_impls)) {
                auto const quals = extract_method_qualifiers(mem);
                auto const spec  = substitute(^^cvts_overload_spec,
                                              {^^Impl,
                                               meta::reflect_constant(impl_m.fn),
                                               meta::reflect_constant(impl_m.is_explicit),
                                               mem});
                methods.push_back(spec);
                if (quals.is_const)
                    methods_c.push_back(spec);
                if (quals.is_volatile)
                    methods_v.push_back(spec);
                if (quals.is_const and quals.is_volatile)
                    methods_cv.push_back(spec);
            };

            auto const add_nonempty = [=](auto& targs, auto& holder_targs) {
                if (holder_targs.size() == 1)
                    return;
                targs.push_back(substitute(^^cvts_holder, holder_targs));
                holder_targs.resize(1);
            };
            add_nonempty(ref_targs, methods);
            add_nonempty(ref_targs_c, methods_c);
            add_nonempty(ref_targs_v, methods_v);
            add_nonempty(ref_targs_cv, methods_cv);
        }
        return std::array{
            substitute(^^cvts_trait_ref_impl, ref_targs),
            substitute(^^cvts_trait_ref_impl, ref_targs_c),
            substitute(^^cvts_trait_ref_impl, ref_targs_v),
            substitute(^^cvts_trait_ref_impl, ref_targs_cv),
        };
    }();

    using ref_impl_t    = [:ref_impls[0]:];
    using ref_c_impl_t  = [:ref_impls[1]:];
    using ref_v_impl_t  = [:ref_impls[2]:];
    using ref_cv_impl_t = [:ref_impls[3]:];

    struct ref : public ref_impl_t {
        using ref_impl_t::ref_impl_t;
    };
    struct ref_c : public ref_c_impl_t {
        using ref_c_impl_t::ref_c_impl_t;
    };
    struct ref_v : public ref_v_impl_t {
        using ref_v_impl_t::ref_v_impl_t;
    };
    struct ref_cv : public ref_cv_impl_t {
        using ref_cv_impl_t::ref_cv_impl_t;
    };
};

template<any_trait Trait, implements_trait<Trait> Impl>
using cvts_trait_ref = [:[] {
    using definer = cvts_ref_definer<std::remove_cvref_t<Trait>, Impl>;
    if constexpr (std::is_const_v<Trait> and std::is_volatile_v<Trait>) {
        return ^^typename definer::ref_cv;
    } else if constexpr (std::is_volatile_v<Trait>) {
        return ^^typename definer::ref_v;
    } else if constexpr (std::is_const_v<Trait>) {
        return ^^typename definer::ref_c;
    } else {
        return ^^typename definer::ref;
    }
}():];

struct name_info_pair {
    char const* name;
    meta::info  member;
};

template<non_cvref CvtsRef>
inline constexpr auto all_cvref_methods = [] {
    auto mems       = std::vector<name_info_pair>{};
    auto types      = std::vector<meta::info>{^^CvtsRef};
    auto next_types = std::vector<meta::info>{};
    while (not stdr::empty(types)) {
        for (auto t: types) {
            mems.append_range(nonstatic_data_members_of(t, unprivileged) | stdv::transform([](meta::info m) {
                                  return name_info_pair{std::define_static_string(identifier_of(m)), m};
                              }));
            next_types.append_range(subextract_base_types(t));
        }
        swap(types, next_types);
        next_types.clear();
    }
    return std::define_static_array(mems);
}();

template<char const* MethodId, non_cvref CvtsRef>
inline constexpr auto ref_method = [] -> meta::info {
    return stdr::find(all_cvref_methods<CvtsRef>,
                      std::string_view(MethodId),
                      [](auto p) { return std::string_view(p.name); })
        ->member;
}();

template<char const* MethodId, typename Ref, typename... Args>
auto call_method_via_id(Ref&& ref, Args&&... args) -> decltype(auto) {
    using ref_t = std::remove_cvref_t<decltype(ref)>;
    using fn_t  = [:[] {
        auto const raw_fn = type_of(ref_method<MethodId, ref_t>);
        if (is_rvalue_reference_type(^^Ref&&)) {
            return add_rvalue_reference(raw_fn);
        } else {
            return add_lvalue_reference(raw_fn);
        }
    }():];

    return static_cast<fn_t>(ref.[:ref_method<MethodId, ref_t>:])(std::forward<Args>(args)...);
}
}    // namespace cvts_trait


// cv-transient static trait reference
template<any_trait Trait, implements_trait<Trait> Impl>
using cvts_trait_ref = cvts_trait::cvts_trait_ref<Trait, Impl>;
}    // namespace trp::detail
