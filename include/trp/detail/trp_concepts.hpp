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
    .exact_ref    = false,
};
inline constexpr auto matching_return_signature = detail::method_signature_requirements_t{
    .exact_return = true,
    .exact_args   = false,
    .exact_cv     = false,
    .exact_ref    = false,
};
inline constexpr auto matching_args_signature = detail::method_signature_requirements_t{
    .exact_return = false,
    .exact_args   = true,
    .exact_cv     = false,
    .exact_ref    = false,
};
inline constexpr auto matching_cv_signature = detail::method_signature_requirements_t{
    .exact_return = false,
    .exact_args   = true,
    .exact_cv     = true,
    .exact_ref    = false,
};
inline constexpr auto exact_signature = detail::method_signature_requirements_t{
    .exact_return = true,
    .exact_args   = true,
    .exact_cv     = false,
    .exact_ref    = false,
};
inline constexpr auto exact_cv_signature = detail::method_signature_requirements_t{
    .exact_return = true,
    .exact_args   = true,
    .exact_cv     = true,
    .exact_ref    = false,
};
inline constexpr auto exact_cvref_signature = detail::method_signature_requirements_t{
    .exact_return = true,
    .exact_args   = true,
    .exact_cv     = true,
    .exact_ref    = true,
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
        .is_lvalue   = is_lvalue_reference_type(ref),
        .is_rvalue   = is_rvalue_reference_type(ref),
        .is_value    = false,
        .is_noexcept = is_noexcept(fn),
    };

    auto idt_targs = std::vector{identifier, meta::reflect_constant(quals), return_type_of(fn)};
    idt_targs.append_range(params | stdv::drop(1) | stdv::transform(meta::type_of));
    return substitute(^^method_identity_t, idt_targs);
}
consteval auto matching_explicit_impl(meta::info impl, meta::info method_idt) -> bool {
    auto       impl_quals = extract_method_qualifiers(impl);
    auto const req_quals  = extract_method_qualifiers(method_idt);
    if (req_quals.is_noexcept and not impl_quals.is_noexcept)
        return false;
    impl_quals.is_noexcept = req_quals.is_noexcept;
    impl                   = replace_method_qualifiers(impl, impl_quals);
    return impl == method_idt or impl == as_lref_method_identity(method_idt);
}

namespace concepts {
template<typename Trait>
concept no_private_members =
    non_cvref<Trait> and not has_inaccessible_nonstatic_data_members(^^Trait, unprivileged);
template<typename Trait>
concept no_private_bases = non_cvref<Trait> and not has_inaccessible_bases(^^Trait, unprivileged);

template<typename Trait>
concept no_nonstatic_data_members =
    non_cvref<Trait>                                                                          //
    and stdr::empty(nonstatic_data_members_of(^^Trait, meta::access_context::unchecked()))    //
    and stdr::empty(static_data_members_of(^^Trait, meta::access_context::unchecked()));

template<typename Trait>
concept no_explicit_special_members = non_cvref<Trait>    //
                                      and stdr::none_of(members_of(^^Trait, unprivileged), [](auto m) {
                                              return is_special_member_function(m)    //
                                                     and is_user_declared(m);
                                          });    //

consteval auto no_operators_fn(std::span<meta::info const> members) -> bool {
    for (auto m: members)
        if (is_operator_function(m) or is_operator_function_template(m))
            return false;
    return true;
}

template<typename Trait>
concept no_operators = non_cvref<Trait> and no_operators_fn(nonspecial_members<Trait>);

consteval auto no_virtual_fn(std::span<meta::info const> rs) {
    for (auto r: rs)
        if (is_virtual(r))
            return false;
    return true;
}

template<typename Trait>
concept no_virtual = non_cvref<Trait>                                //
                     and no_virtual_fn(nonspecial_members<Trait>)    //
                     and no_virtual_fn(bases_of(^^Trait, unprivileged));

template<typename Trait>
concept nonstatic_methods_are_not_templates =
    non_cvref<Trait> and stdr::none_of(nonspecial_members<Trait>, [](auto r) {
        return is_function_template(r) and not is_static_member(r);
    });

template<typename Trait>
concept no_vararg_methods =
    non_cvref<Trait> and stdr::none_of(nonspecial_members<Trait>, meta::is_vararg_function);

consteval auto methods_have_no_default_args(std::span<meta::info const> methods) {
    for (auto m: methods) {
        if (not is_function(m))
            continue;
        for (auto p: parameters_of(m))
            if (has_default_argument(p))
                return false;
    }
    return true;
}

template<typename Trait>
concept no_default_arguments = non_cvref<Trait> and methods_have_no_default_args(nonspecial_members<Trait>);

consteval auto static_methods_are_templates_fn(std::span<meta::info const> members) {
    for (auto m: members)
        if (is_static_member(m) and is_function(m))
            return false;
    return true;
}

template<typename Trait>
concept static_methods_are_templates =
    non_cvref<Trait> and static_methods_are_templates_fn(nonspecial_members<Trait>);

consteval auto valid_methods_fn(meta::info trait, std::span<meta::info const> methods) {
    for (auto m: methods) {
        if (not extract<bool>(substitute(^^trait_method_idt, {m})))
            return false;
    }
    return true;
}

template<typename T>
concept valid_methods = valid_methods_fn(^^T, direct_trait_methods<T>);

consteval auto static_methods_are_default_impl_for_fn(std::span<meta::info const> members,
                                                      std::span<meta::info const> all_methods,
                                                      meta::info                  trait) -> bool {
    for (auto member: members) {
        if (not(is_static_member(member) and is_function_template(member)))
            continue;
        auto const impl_idt = explicit_impl_to_method_identity(member, trait);
        auto       matching = false;
        for (auto m: all_methods) {
            if (matching_explicit_impl(impl_idt, m)) {
                matching = true;
                break;
            }
        }
        if (not matching)
            return false;
    }
    return true;
}

template<typename T, typename Trait>
concept static_methods_are_default_impl_for =
    non_cvref<T>            //
    and non_cvref<Trait>    //
    and static_methods_are_default_impl_for_fn(nonspecial_members<Trait>, all_trait_methods<Trait>, ^^Trait);


template<typename Trait>
concept any_immediate_trait = non_cvref<Trait>                                         //
                              and no_private_members<Trait>                            //
                              and no_private_bases<Trait>                              //
                              and no_virtual<Trait>                                    //
                              and no_nonstatic_data_members<Trait>                     //
                              and no_explicit_special_members<Trait>                   //
                              and static_methods_are_templates<Trait>                  //
                              and no_operators<Trait>                                  //
                              and nonstatic_methods_are_not_templates<Trait>           //
                              and no_vararg_methods<Trait>                             //
                              and no_default_arguments<Trait>                          //
                              and valid_methods<Trait>                                 //
                              and static_methods_are_default_impl_for<Trait, Trait>    //
    ;

consteval auto recurse_substitute(meta::info r, std::span<meta::info const> args) {
    if (args.empty())
        return true;
    auto s_args = std::vector{meta::reflect_constant(r)};
    s_args.append_range(args);
    return extract<bool>(substitute(r, s_args));
}

// template<meta::info Self, typename... Traits>
// concept any_traits = (true and ... and
//                       (any_immediate_trait<std::remove_cv_t<Traits>> and
//                        recurse_substitute(Self, direct_base_types<std::remove_cv_t<Traits>>)));

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


consteval auto supertrait_of_fn(std::span<meta::info const> supertrat_methods,
                                std::span<meta::info const> trait_methods) -> bool {
    for (auto m: supertrat_methods) {
        if (not contains_submethod_of(trait_methods, m))
            return false;
    }
    return true;
}

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
                        and detail::concepts::supertrait_of_fn(detail::all_trait_methods<Supertrait>,
                                                               detail::all_trait_methods<Trait>);

template<typename Supertrait, typename Trait>
concept direct_supertrait_of =
    supertrait_of<Supertrait, Trait> and
    stdr::contains(detail::direct_base_types<std::remove_cv_t<Trait>>, remove_cv(^^Supertrait));

template<typename Supertrait, typename Trait>
concept explicit_supertrait_of = supertrait_of<Supertrait, Trait> and std::derived_from<Trait, Supertrait>;


namespace detail {
struct is_matching_id {
    std::string_view id_sv;
    consteval bool   operator()(meta::info r) const {
        return has_identifier(r) and identifier_of(r) == id_sv;
    }
};
template<non_cvref Impl, char const* Id>
inline constexpr auto matching_id_direct_public_members =
    std::define_static_array(members_of(^^Impl, unprivileged)    //
                             | stdv::filter(is_matching_id{Id}));

template<typename Impl, meta::info ImplMethod, bool Noexcept, typename... Args>
inline constexpr bool invocable_from_idt = [] {
    if constexpr (meta::is_template(ImplMethod)) {
        if constexpr (Noexcept) {
            return requires {
                { std::declval<Impl>().template[:ImplMethod:](std::declval<Args>()...) } noexcept;
            };
        } else {
            return requires {
                { std::declval<Impl>().template[:ImplMethod:](std::declval<Args>()...) };
            };
        }
    } else {
        if constexpr (Noexcept) {
            return requires {
                { std::declval<Impl>().[:ImplMethod:](std::declval<Args>()...) } noexcept;
            };
        } else {
            return requires {
                { std::declval<Impl>().[:ImplMethod:](std::declval<Args>()...) };
            };
        }
    }
}();


template<typename Impl, meta::info ImplMethod, typename... Args>
using method_return_t = [:[] {
    if constexpr (is_template(ImplMethod)) {
        return ^^decltype(std::declval<Impl>().template[:ImplMethod:](std::declval<Args>()...));
    } else {
        return ^^decltype(std::declval<Impl>().[:ImplMethod:](std::declval<Args>()...));
    }
}():];

consteval auto invocable_as_method(meta::info impl, /**/
                                   meta::info impl_method,
                                   meta::info method_idt,
                                   bool       exact_ret) -> bool {
    auto const params = extract_method_param_types(method_idt);
    auto const ret    = extract_method_return_type(method_idt);
    auto const quals  = extract_method_qualifiers(method_idt);

    auto invk_impl = impl;
    if (quals.is_const)
        invk_impl = add_const(invk_impl);
    if (quals.is_volatile)
        invk_impl = add_volatile(invk_impl);
    if (quals.is_rvalue)
        invk_impl = add_rvalue_reference(invk_impl);
    else
        invk_impl = add_lvalue_reference(invk_impl);

    auto const method_r = reflect_constant(impl_method);

    auto invk_args = std::vector{invk_impl, method_r, meta::reflect_constant(quals.is_noexcept)};
    invk_args.append_range(params);
    if (not extract<bool>(substitute(^^invocable_from_idt, invk_args)))
        return false;

    auto invk_ret_args = std::vector{invk_impl, method_r};
    invk_ret_args.append_range(params);
    auto const invk_ret    = substitute(^^method_return_t, invk_ret_args);
    auto const same_return = extract<bool>(substitute(^^std::same_as, {invk_ret, ret}));
    if (same_return)
        return true;
    if (exact_ret)
        return false;
    if (quals.is_noexcept)
        return extract<bool>(substitute(^^std::is_nothrow_convertible_v, {invk_ret, ret}));
    return extract<bool>(substitute(^^std::is_convertible_v, {invk_ret, ret}));
}


consteval auto call_operators_of_fn(meta::info type) -> std::vector<meta::info> {
    auto v = members_of(type, unprivileged) | stdv::filter([](auto m) {
                 return (is_operator_function(m) or is_operator_function_template(m)) and
                        operator_of(m) == meta::operators::op_parentheses;
             }) |
             stdr::to<std::vector>();
    if (v.empty()) {
        for (auto base: subextract_base_types(type))
            v.append_range(call_operators_of_fn(base));
    }
    return v;
};
template<typename T>
inline constexpr auto call_operators_of = std::define_static_array(call_operators_of_fn(^^T));

template<typename MPtr, meta::info ImplMethod>
concept extractable_template = requires(MPtr ptr) {
    { ptr = &template[:ImplMethod:] };
};

struct match {
    bool args{};
    bool cv{};
    bool ref{};

    [[nodiscard]] constexpr auto full_match() const -> bool {
        return args and cv and ref;
    }
    [[nodiscard]] constexpr auto satisfies(method_signature_requirements_t reqs) const -> bool {
        return (args or not reqs.exact_args) and (cv or not reqs.exact_cv) and (ref or not reqs.exact_ref);
    };
};

consteval auto check_parameter_match(meta::info impl, meta::info impl_method, meta::info method_idt)
    -> match {
    if (not(is_function(impl_method) or is_function_template(impl_method)))
        throw "Expected function or function template";
    auto const trait_params = extract_method_param_types(method_idt);
    auto const quals        = extract_method_qualifiers(method_idt);

    auto const add_method_cvref = [=](meta::info r) {
        r = add_method_obj_cv(method_idt, r);
        if (not quals.is_rvalue)
            r = add_lvalue_reference(r);
        return r;
    };
    auto const invokation_info = add_method_cvref(impl);

    // Return type matching is performed earlier.
    // This way the return type rules are decoupled from argument rules.
    // noexcept promotion in pointer conversion is automatic

    if (is_function(impl_method)) {
        auto const impl_params_raw = parameters_of(impl_method);
        auto const is_eop = not impl_params_raw.empty() and is_explicit_object_parameter(impl_params_raw[0]);
        auto const impl_params = [&] {
            if (is_eop) {
                return impl_params_raw | stdv::drop(1) | stdv::take(trait_params.size()) |
                       stdv::transform(meta::type_of) | stdr::to<std::vector>();
            };
            return impl_params_raw | stdv::take(trait_params.size()) | stdv::transform(meta::type_of) |
                   stdr::to<std::vector>();
        }();
        if (not stdr::equal(trait_params, impl_params))
            return {};
        if (is_static_member(impl_method))
            return match{
                .args = true,
                .cv   = quals.is_const and quals.is_volatile,
                .ref  = not quals.is_ref(),
            };
        auto res = match{.args = true};

        if (is_eop) {
            auto const eop_t = type_of(impl_params_raw[0]);
            res.cv |= (quals.is_const == is_const(remove_reference(eop_t)))    //
                      and (quals.is_volatile == is_volatile(remove_reference(eop_t)));
        } else {
            res.cv =
                (quals.is_const == is_const(impl_method)) and (quals.is_volatile == is_volatile(impl_method));
        }
        auto const impl_is_lvalue = is_eop ? is_lvalue_reference_type(type_of(impl_params_raw[0]))
                                           : is_lvalue_reference_qualified(impl_method);
        auto const impl_is_rvalue = is_eop ? is_rvalue_reference_type(type_of(impl_params_raw[0]))
                                           : is_rvalue_reference_qualified(impl_method);

        if (quals.is_rvalue and impl_is_lvalue or not quals.is_rvalue and impl_is_rvalue)
            return {};
        res.ref = quals.is_lvalue == impl_is_lvalue and quals.is_rvalue == impl_is_rvalue;
        return res;
    } else if (is_function_template(impl_method)) {
        auto const method_r    = reflect_constant(impl_method);
        auto const return_type = [&] {
            auto targs = std::vector{invokation_info, method_r};
            targs.append_range(trait_params);
            return substitute(^^method_return_t, targs);
        }();
        auto const get_fptr_t = [&](bool eop, meta::info obj_t) {
            auto targs = std::vector{
                meta::reflect_constant(eop), obj_t, return_type, meta::reflect_constant(quals.is_noexcept)};
            targs.append_range(trait_params);
            return substitute(^^make_function_member_type, targs);
        };
        auto const is_extractable = [=](meta::info fptr_t) {
            return extract<bool>(substitute(^^extractable_template, {fptr_t, method_r}));
        };

        auto const obj     = add_method_obj_cv(method_idt, impl);
        auto const obj_ref = add_method_obj_cvref(method_idt, impl);

        auto const mem_fptr = get_fptr_t(false, obj_ref);
        if (is_extractable(mem_fptr))
            return {.args = true, .cv = true, .ref = true};

        if (quals.is_ref()) {
            auto const mem_fptr = get_fptr_t(false, obj);
            if (is_extractable(mem_fptr))
                return {.args = true, .cv = true, .ref = true};

            auto const eop_fptr = get_fptr_t(true, obj_ref);
            if (is_extractable(eop_fptr))
                return {.args = true, .cv = true, .ref = true};
        }

        if (not quals.is_rvalue) {
            auto const mem_fptr = get_fptr_t(false, add_lvalue_reference(obj));
            if (is_extractable(mem_fptr))
                return {.args = true, .cv = true, .ref = false};
        }

        if (not quals.is_ref()) {
            auto const mem_oep_fptr = get_fptr_t(true, add_lvalue_reference(obj));
            if (is_extractable(mem_oep_fptr))
                return {.args = true, .cv = true, .ref = false};
        } else if (not quals.is_volatile and not is_volatile(impl)) {
            auto const eop_fptr = get_fptr_t(true, obj);
            if (is_extractable(eop_fptr))
                return {.args = true, .cv = true, .ref = false};
        }

        auto invk_args = std::vector{meta::info{}, method_r, meta::reflect_constant(quals.is_noexcept)};
        invk_args.append_range(trait_params);
        auto const is_invocable = [&](meta::info impl) {
            invk_args[0] = impl;
            return extract<bool>(substitute(^^invocable_from_idt, invk_args));
        };
        // Using more relaxed of concepts to verify that cv qualifiers do not break invokability.
        if (not meta::is_const(remove_reference(invokation_info)) and is_invocable(add_const(impl))) {
            auto with_const = check_parameter_match(add_const(impl), impl_method, method_idt);
            with_const.cv   = false;
            return with_const;
        }
        // Using more relaxed of concepts to verify that cv qualifiers do not break invokability.
        if (not meta::is_volatile(remove_reference(invokation_info)) and is_invocable(add_volatile(impl))) {
            auto with_volatile = check_parameter_match(add_volatile(impl), impl_method, method_idt);
            with_volatile.cv   = false;
            return with_volatile;
        }
        return {};
    }
    return {};
}

consteval bool first_parameter_is_non_eop_cvref_of(meta::info fn, meta::info param_type) {
    auto const params = parameters_of(fn);
    return stdr::size(params) > 0                             //
           and is_reference_type(type_of(params[0]))          //
           and not is_explicit_object_parameter(params[0])    //
           and remove_cvref(type_of(params[0])) == dealias(param_type);
}
}    // namespace detail
}    // namespace trp
