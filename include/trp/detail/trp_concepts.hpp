#pragma once
#ifndef TRP_GODBOLT
#include "alias_and_helpers.hpp"
#include "cvtmock_trait_ref.hpp"
#endif

namespace trp {

inline constexpr auto relaxed_signature = detail::method_signature_requirements_t{
    .exact_return = false,
    .exact_args   = false,
    .exact_cv     = false,

};
inline constexpr auto matching_return_signature = detail::method_signature_requirements_t{
    .exact_return = true,
    .exact_args   = false,
    .exact_cv     = false,
};
inline constexpr auto matching_args_signature = detail::method_signature_requirements_t{
    .exact_return = false,
    .exact_args   = true,
    .exact_cv     = false,
};
inline constexpr auto matching_cv_signature = detail::method_signature_requirements_t{
    .exact_return = false,
    .exact_args   = true,
    .exact_cv     = true,
};
inline constexpr auto exact_signature = detail::method_signature_requirements_t{
    .exact_return = true,
    .exact_args   = true,
    .exact_cv     = false,
};
inline constexpr auto exact_cv_signature = detail::method_signature_requirements_t{
    .exact_return = true,
    .exact_args   = true,
    .exact_cv     = true,
};

namespace detail {

consteval auto explicit_impl_to_method_identity(meta::info fn, meta::info trait_inf) {
    auto const identifier = meta::reflect_constant_string(identifier_of(fn));
    if (is_template(fn)) {
        auto const mock_inf = substitute(^^mock_trait_ref, {trait_inf});
        fn                  = substitute(fn, {mock_inf});
    }
    auto const params = parameters_of(fn);
    if (stdr::empty(params))
        throw "Explicit trait implementation functions must accept an implementation object reference as a "
              "first parameter.";
    auto const ref   = type_of(params[0]);
    auto const quals = method_qualifiers_t{
        .is_const    = is_const(remove_reference(ref)),
        .is_volatile = is_volatile(remove_reference(ref)),
        .is_lvalue   = true,
        .is_rvalue   = false,
        .is_value    = false,
        .is_noexcept = is_noexcept(fn),
    };

    auto idt_targs = std::vector{identifier, meta::reflect_constant(quals), return_type_of(fn)};
    idt_targs.append_range(params | stdv::drop(1) | stdv::transform(meta::type_of));
    return substitute(^^method_identity_t, idt_targs);
}
consteval auto matching_explicit_impl(meta::info impl, meta::info method_idt) -> bool {
    return impl == method_idt or impl == as_lref_method_identity(method_idt);
}

namespace concepts {

template<typename T, meta::info M>
consteval bool check_constexpr_static_data_member() {
#if defined(__GNUC__) && !defined(__clang__)
#define TRP_CHECK_CESDM
#endif

#ifdef TRP_CHECK_CESDM
    return requires { cw<([:M:])>; };
#else
    return false;
#endif
#undef TRP_CHECK_CESDM
}

template<typename Trait>
concept no_private_members =
    non_cvref<Trait> and stdr::size(members_of(^^Trait, meta::access_context::unchecked())) ==
                             stdr::size(members_of(^^Trait, unprivileged));
template<typename Trait>
concept no_private_bases = non_cvref<Trait> and not has_inaccessible_bases(^^Trait, unprivileged);

template<typename Trait>
concept no_nonstatic_data_members =
    non_cvref<Trait> and stdr::empty(nonstatic_data_members_of(^^Trait, unprivileged));

template<typename Trait>
concept no_explicit_special_members = non_cvref<Trait>    //
                                      and stdr::none_of(members_of(^^Trait, unprivileged), [](auto m) {
                                              return is_special_member_function(m)    //
                                                     and not is_defaulted(m);
                                          });    //
//
template<typename Trait>
concept no_operators = non_cvref<Trait>    //
                       and stdr::none_of(nonspecial_members<Trait>, [](auto m) {
                               return is_operator_function(m)    //
                                      or is_operator_function_template(m);
                           });    //
template<typename Trait>
concept no_virtual =
    non_cvref<Trait>                                                  //
    and stdr::none_of(nonspecial_members<Trait>, meta::is_virtual)    //
    and stdr::none_of(bases_of(^^Trait, meta::access_context::unchecked()), meta::is_virtual);

template<typename Trait>
concept nonstatic_methods_are_not_templates =
    non_cvref<Trait> and stdr::none_of(nonspecial_members<Trait>, [](auto r) {
        return is_function_template(r) and not is_static_member(r);
    });
template<typename Trait>
concept static_methods_are_templates =
    non_cvref<Trait> and
    stdr::none_of(nonspecial_members<Trait>, [](auto r) { return is_static_member(r) and is_function(r); });

template<typename T>
concept valid_methods = [] {
    for (auto m: direct_trait_methods<T>) {
        if (not extract<bool>(substitute(^^trait_method_idt, {m})))
            return false;
    }
    return true;
}();

template<typename T>
concept static_data_members_are_constexpr = [] {
    constexpr auto mems = std::define_static_array(static_data_members_of(^^T, unprivileged));
    auto [... Is]       = make_cw_idxs<mems.size()>();
    return (check_constexpr_static_data_member<T, mems[Is]>() and ... and true);
}();

template<typename T, typename Trait>
concept static_methods_are_default_impl_for =
    non_cvref<T>            //
    and non_cvref<Trait>    //
    and stdr::all_of(
            nonspecial_members<T> |
                stdv::filter([](auto r) { return is_static_member(r) and (is_function_template(r)); }),
            [](auto r) {
                return stdr::contains(all_trait_methods<Trait> | stdv::transform(as_lref_method_identity),
                                      explicit_impl_to_method_identity(r, ^^Trait));
            });


template<typename Trait>
concept any_immediate_trait = non_cvref<Trait>                                         //
                              and no_private_members<Trait>                            //
                              and no_private_bases<Trait>                              //
                              and no_virtual<Trait>                                    //
                              and no_nonstatic_data_members<Trait>                     //
                              and no_explicit_special_members<Trait>                   //
                              and static_data_members_are_constexpr<Trait>             // only in gcc atm
                              and static_methods_are_templates<Trait>                  //
                              and no_operators<Trait>                                  //
                              and nonstatic_methods_are_not_templates<Trait>           //
                              and valid_methods<Trait>                                 //
                              and static_methods_are_default_impl_for<Trait, Trait>    //
    ;

template<meta::info Self, typename... Traits>
concept any_traits =
    (... and (any_immediate_trait<std::remove_cv_t<Traits>> and
              [:substitute(Self,
                           [] {
                               auto args = std::vector{meta::reflect_constant(Self)};
                               args.append_range(direct_base_types<std::remove_cv_t<Traits>>);
                               return args;
                           }()):])    //
    );

}    // namespace concepts
}    // namespace detail

template<typename Trait>
concept any_trait =
    std::is_class_v<Trait> and detail::concepts::any_traits<^^detail::concepts::any_traits, Trait>;

template<typename Trait>
concept non_cv_trait = non_cvref<Trait> and any_trait<Trait>;

template<typename Supertrait, typename Trait>
concept supertrait_of = any_trait<Supertrait>    //
                        and any_trait<Trait>     //
                        and ([] {
                                for (auto m: detail::all_trait_methods<Supertrait>) {
                                    if (not stdr::contains(detail::all_trait_methods<Trait>, m))
                                        return false;
                                }
                                return true;
                            }());
template<typename Supertrait, typename Trait>
concept direct_supertrait_of =
    supertrait_of<Supertrait, Trait> and
    stdr::contains(detail::direct_base_types<std::remove_cv_t<Trait>>, remove_cv(^^Supertrait));
template<typename Supertrait, typename Trait>
concept explicit_supertrait_of = supertrait_of<Supertrait, Trait> and std::derived_from<Trait, Supertrait>;


namespace detail {
template<non_cvref Impl, auto Id>
inline constexpr auto matching_id_direct_public_members = std::define_static_array(
    members_of(^^Impl, unprivileged)    //
    | stdv::filter([](auto info) { return has_identifier(info) and identifier_of(info) == Id; }));

template<non_ref Impl, meta::info ImplMethod, any_method_idt MethodIdt>
inline constexpr bool invokable_exact_return = [] {
    auto [... arg_ids]      = MethodIdt::param_identities;
    using return_type       = MethodIdt::return_type;
    using impl_invocation_t = [:MethodIdt::add_obj_cv(^^Impl):];

    if constexpr (meta::is_template(ImplMethod)) {
        if constexpr (MethodIdt::is_noexcept) {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.template[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } noexcept -> std::same_as<return_type>;
            };
        } else {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.template[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } -> std::same_as<return_type>;
            };
        }
    } else {
        if constexpr (MethodIdt::is_noexcept) {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } noexcept -> std::same_as<return_type>;
            };
        } else {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } -> std::same_as<return_type>;
            };
        }
    }
}();
template<non_ref Impl, meta::info ImplMethod, any_method_idt MethodIdt>
inline constexpr bool invokable_convert_return = [] {
    auto [... arg_ids]      = MethodIdt::param_identities;
    using return_type       = MethodIdt::return_type;
    using impl_invocation_t = [:MethodIdt::add_obj_cv(^^Impl):];

    if constexpr (meta::is_template(ImplMethod)) {
        if constexpr (MethodIdt::is_noexcept) {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.template[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } noexcept -> std::convertible_to<return_type>;
            };
        } else {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.template[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } -> std::convertible_to<return_type>;
            };
        }
    } else {
        if constexpr (MethodIdt::is_noexcept) {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } noexcept -> std::convertible_to<return_type>;
            };
        } else {
            return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
                {
                    impl.[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
                } -> std::convertible_to<return_type>;
            };
        }
    }
}();


template<non_cv_trait Trait>
inline constexpr auto method_signature_requirements = [] -> method_signature_requirements_t {
    if (auto o_req = extract_signature_req(^^Trait); o_req)
        return *o_req;
    return {};
}();

template<non_ref Impl, meta::info ImplMethod, typename... Args>
using method_return_t = [:[] {
    if constexpr (is_template(ImplMethod)) {
        return ^^decltype(std::declval<Impl>().template[:ImplMethod:](std::declval<Args>()...));
    } else {
        return ^^decltype(std::declval<Impl>().[:ImplMethod:](std::declval<Args>()...));
    }
}():];
template<typename T>
inline constexpr auto call_operators_of =
    std::define_static_array(members_of(^^T, unprivileged) | stdv::filter([](auto m) {
                                 return (is_operator_function(m) or is_operator_function_template(m)) and
                                        operator_of(m) == meta::operators::op_parentheses;
                             }));


template<non_ref Impl, meta::info ImplMethod, trait_method_idt MethodIdt, bool ExactCv>
inline constexpr auto exactly_matches = [] {
    auto [... arg_ids]      = MethodIdt::param_identities;
    using impl_invocation_t = [:MethodIdt::add_obj_cv(^^Impl):];

    // Return type matching is performed earlier.
    // This way the return type rules are decoupled from argument rules.
    constexpr auto m_refl  = std::meta::reflect_constant(ImplMethod);
    constexpr auto cv_refl = meta::reflect_constant(ExactCv);

    using return_type    = [:[] {
        if (invokable_convert_return<impl_invocation_t, ImplMethod, MethodIdt>)
            return substitute(^^method_return_t,
                                 {^^impl_invocation_t, m_refl, ^^typename decltype(arg_ids)::type...});
        return ^^void;
    }():];
    using exact_type     = make_function_member_type<false,
                                                     impl_invocation_t,
                                                     return_type,
                                                     MethodIdt::is_noexcept,
                                                     typename decltype(arg_ids)::type...>;
    using exact_eop_type = make_function_member_type<true,
                                                     impl_invocation_t&,
                                                     return_type,
                                                     MethodIdt::is_noexcept,
                                                     typename decltype(arg_ids)::type...>;

    // noexcept promotion in pointer conversion is automatic

    // possibly reference promotion for non-eop methods for `foo() => foo() &` and `foo() => foo() &&`
    // not needed for now since reference qualification for trait methods is explicitly not allowed

    // add static function resolution?
    if constexpr (not is_function(ImplMethod) and not is_function_template(ImplMethod)) {
        auto const call_ops        = subextract_info_span(^^call_operators_of, {type_of(ImplMethod)});
        auto const method_inv_type = copy_cv_to(^^impl_invocation_t, type_of(ImplMethod));
        return stdr::any_of(call_ops, [=](auto op) {
            return extract<bool>(substitute(
                ^^exactly_matches, {method_inv_type, meta::reflect_constant(op), ^^MethodIdt, cv_refl}));
        });
    } else {
        auto const exact_method = (dealias(^^exact_type) != ^^void)    //
            and requires(exact_type mptr) {
                { mptr = &[:ImplMethod:] };
            };
        auto const exact_eop_method = (dealias(^^exact_eop_type) != ^^void)    //
            and requires(exact_eop_type mptr) {
                { mptr = &[:ImplMethod:] };
            };

        if (exact_method or exact_eop_method)
            return true;
        
        if (ExactCv)
            return false;

        // Using more relaxed of concepts to verify that cv qualifiers do not break invokability.
        if constexpr (not meta::is_const(remove_reference(^^impl_invocation_t)) and
                      invokable_convert_return<impl_invocation_t const, ImplMethod, MethodIdt>) {
            auto const exact_method     = extract<bool>(substitute(
                ^^exactly_matches, {add_const(^^impl_invocation_t), m_refl, ^^MethodIdt, cv_refl}));
            auto const exact_eop_method = extract<bool>(substitute(
                ^^exactly_matches, {add_const(^^impl_invocation_t), m_refl, ^^MethodIdt, cv_refl}));
            if (exact_method or exact_eop_method)
                return true;
        }
        // Using more relaxed of concepts to verify that cv qualifiers do not break invokability.
        if constexpr (not meta::is_volatile(remove_reference(^^impl_invocation_t)) and
                      invokable_convert_return<impl_invocation_t volatile, ImplMethod, MethodIdt>) {
            auto const exact_method     = extract<bool>(substitute(
                ^^exactly_matches, {add_volatile(^^impl_invocation_t), m_refl, ^^MethodIdt, cv_refl}));
            auto const exact_eop_method = extract<bool>(substitute(
                ^^exactly_matches, {add_volatile(^^impl_invocation_t), m_refl, ^^MethodIdt, cv_refl}));
            if (exact_method or exact_eop_method)
                return true;
        }
        return false;
    }
}();


template<typename ParamType>
consteval bool first_parameter_is_non_eop_cvref_of(meta::info fn) {
    auto const params = parameters_of(fn);
    return stdr::size(params) > 0                             //
           and is_reference_type(type_of(params[0]))          //
           and not is_explicit_object_parameter(params[0])    //
           and remove_cvref(type_of(params[0])) == ^^ParamType;
}
}    // namespace detail
}    // namespace trp
