#pragma once
#ifndef TRP_GODBOLT
#include "alias_and_helpers.hpp"
#include "cvtmock_trait_ref.hpp"
#endif

namespace trp {
namespace detail {

consteval auto explicit_impl_to_method_identity(meta::info fn) {
    using namespace meta;

    const auto identifier = reflect_constant_string(identifier_of(fn));
    if (is_template(fn)) {
        const auto trait_inf = parent_of(fn);
        const auto mock_inf  = substitute(^^mock_trait_ref, {trait_inf});
        fn                   = substitute(fn, {mock_inf});
    }
    const auto params = parameters_of(fn);
    if (stdr::empty(params))
        throw "Default trait implementation functions must accept at implementation object refernce as a "
              "first paramter.";
    const auto impl = type_of(params[0]);

    auto idt_targs = std::vector{identifier,
                                 reflect_constant(is_const(remove_reference(impl))),
                                 reflect_constant(is_volatile(remove_reference(impl))),
                                 reflect_constant(false),
                                 reflect_constant(false),
                                 reflect_constant(false),
                                 reflect_constant(is_noexcept(fn)),
                                 return_type_of(fn)};
    idt_targs.append_range(params | stdv::drop(1) | stdv::transform(type_of));
    return substitute(^^method_identity_t, idt_targs);
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
}

template<typename Trait>
concept no_private_members =
    non_cvref<Trait> and stdr::size(meta::members_of(^^Trait, meta::access_context::unchecked())) ==
                             stdr::size(meta::members_of(^^Trait, meta::access_context::unprivileged()));
template<typename Trait>
concept no_private_bases =
    non_cvref<Trait> and stdr::size(direct_base_types<Trait>) ==
                             stdr::size(meta::bases_of(^^Trait, meta::access_context::unprivileged()));
template<typename Trait>
concept no_nonstatic_data_members =
    non_cvref<Trait> and
    stdr::empty(meta::nonstatic_data_members_of(^^Trait, meta::access_context::unprivileged()));

template<typename Trait>
concept no_explicit_special_members =
    non_cvref<Trait>    //
    and stdr::none_of(meta::members_of(^^Trait, meta::access_context::unprivileged()), [](auto m) {
            return meta::is_special_member_function(m)    //
                   and not meta::is_defaulted(m);
        });    //
//
template<typename Trait>
concept no_operators = non_cvref<Trait>    //
                       and stdr::none_of(nonspecial_members<Trait>, [](auto m) {
                               return meta::is_operator_function(m)    //
                                      or meta::is_operator_function_template(m);
                           });    //
template<typename Trait>
concept no_virtual = non_cvref<Trait>                                                  //
                     and stdr::none_of(nonspecial_members<Trait>, meta::is_virtual)    //
                     and stdr::none_of(direct_base_types<Trait>, meta::is_virtual);

template<typename Trait>
concept nonstatic_methods_are_not_templates =
    non_cvref<Trait> and stdr::none_of(nonspecial_members<Trait>, [](auto r) {
        return meta::is_function_template(r) and not meta::is_static_member(r);
    });
template<typename Trait>
concept static_methods_are_templates =
    non_cvref<Trait> and stdr::none_of(nonspecial_members<Trait>, [](auto r) {
        return meta::is_static_member(r) and not meta::is_function_template(r);
    });

template<typename T>
concept static_data_members_are_constexpr = [] {
    constexpr auto mems = std::define_static_array(meta::static_data_members_of(^^T, ctx_unchecked));
    auto [... Is]       = make_cw_idxs<mems.size()>();
    return (check_constexpr_static_data_member<T, mems[Is]>() and ... and true);
}();

template<typename T, typename Trait>
concept static_methods_are_default_impl_for =
    non_cvref<T>            //
    and non_cvref<Trait>    //
    and stdr::all_of(nonspecial_members<T> | stdv::filter([](auto r) {
                         return meta::is_static_member(r)             //
                                and (meta::is_function_template(r)    //
                                     or meta::is_function(r));
                     }),
                     [](auto r) {
                         return stdr::contains(all_trait_methods<Trait>, explicit_impl_to_method_identity(r));
                     });


template<typename Trait>
concept any_immediate_trait = non_cvref<Trait>                                         //
                              and no_private_members<Trait>                            //
                              and no_private_bases<Trait>                              //
                              and no_virtual<Trait>                                    //
                              and no_nonstatic_data_members<Trait>                     //
                              and no_explicit_special_members<Trait>                   //
                              and static_data_members_are_constexpr<Trait>             // only in gcc atm
                              and no_operators<Trait>                                  //
                              and nonstatic_methods_are_not_templates<Trait>           //
                              and static_methods_are_templates<Trait>                  //
                              and static_methods_are_default_impl_for<Trait, Trait>    //
    ;

template<meta::info Self, typename... Traits>
concept any_traits =
    (... and (any_immediate_trait<std::remove_cv_t<Traits>> and
              [:meta::substitute(Self,
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
concept non_cv_trait = any_trait<Trait> and non_cvref<Trait>;

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
    stdr::contains(detail::direct_base_types<std::remove_cv_t<Trait>>, meta::remove_cv(^^Supertrait));
template<typename Supertrait, typename Trait>
concept explicit_supertrait_of = supertrait_of<Supertrait, Trait> and std::derived_from<Trait, Supertrait>;


namespace detail {
template<non_cvref Impl, auto Id>
inline constexpr auto matching_id_direct_public_members = std::define_static_array(
    meta::members_of(^^Impl, meta::access_context::unprivileged())    //
    | stdv::filter([](auto info) { return meta::has_identifier(info) and meta::identifier_of(info) == Id; }));

template<non_cvref Impl, auto Id>
inline constexpr auto matching_id_public_members = [] {
    using namespace meta;
    auto result = matching_id_direct_public_members<Impl, Id>    //
                  | stdr::to<std::vector<info>>();
    template for (constexpr auto base: direct_base_types<Impl>) {
        using base_t = [:base:];
        for (auto m: matching_id_public_members<base_t, Id>)
            if (not stdr::contains(result, m))
                result.push_back(m);
    }
    return std::define_static_array(result);
}();

template<non_ref Impl, meta::info ImplMethod, trait_method_idt MethodId>
inline constexpr bool strictly_matches = [] {
    using return_type       = MethodId::return_type;
    using impl_invocation_t = [:MethodId::add_obj_cv(^^Impl):];
    auto [... arg_ids]      = MethodId::param_identities;
    if constexpr (meta::is_template(ImplMethod)) {
        return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
            {
                impl.template[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
            } -> std::same_as<return_type>;
        };
    } else {
        return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
            {
                impl.[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
            } -> std::same_as<return_type>;
        };
    }
}();

template<non_ref Impl, trait_method_idt TraitMethod>
inline constexpr auto matching_impl_method = [] {
    for (auto m: matching_id_public_members<std::remove_cv_t<Impl>, TraitMethod::identifier>) {
        auto matches =
            meta::substitute(^^strictly_matches, {^^Impl, meta::reflect_constant(m), ^^TraitMethod});
        if (meta::extract<bool>(matches))
            return m;
    }
    return meta::info{};
}();


template<typename ParamType>
consteval bool first_parameter_is_cvref_of(meta::info fn) {
    auto params = meta::parameters_of(fn);
    return stdr::size(params) > 0                                   //
           and meta::is_reference_type(meta::type_of(params[0]))    //
           and meta::remove_cvref(meta::type_of(params[0])) == ^^ParamType;
}
}    // namespace detail
}    // namespace trp
