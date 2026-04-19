#pragma once
#ifndef TRP_GODBOLT
#include "cvtmock_trait_ref.hpp"
#endif

namespace trp {
namespace detail {

template<non_cv_trait Trait>
struct default_trait_impl_identity;

template<non_cv_trait Trait, non_ref Impl>
using default_trait_impl_for = [:substitute(
                                     decltype(default_trait_impl_identity<Trait>::template_info)::value,
                                     {^^Impl}):];

consteval auto default_impl_to_method_identity(meta::info fn) {
    using namespace meta;
    auto params    = parameters_of(fn);
    auto impl      = remove_reference(type_of(params[0]));
    auto idt_targs = std::vector{reflect_constant_string(identifier_of(fn)),
                                 reflect_constant(is_const(impl)),
                                 reflect_constant(is_volatile(impl)),
                                 reflect_constant(false),
                                 reflect_constant(false),
                                 reflect_constant(false),
                                 reflect_constant(is_noexcept(fn)),
                                 return_type_of(fn)};
    idt_targs.append_range(params | stdv::drop(1));
    return substitute(^^method_identity_t, idt_targs);
}

template<typename T>
struct empty_default_implementation {};

template<typename ParamType>
consteval bool first_parameter_is_cvref_of(meta::info fn) {
    auto params = meta::parameters_of(fn);
    return stdr::size(params) > 0                                   //
           and meta::is_reference_type(meta::type_of(params[0]))    //
           and meta::remove_cvref(meta::type_of(params[0])) == ^^ParamType;
}
template<any_trait Trait>
consteval bool maps_to_a_trait_method_of(meta::info fn) {
    return stdr::contains(all_trait_methods<Trait>, default_impl_to_method_identity(fn));
};

template<template<typename> typename DefaultImpl, typename Trait>
concept default_impl_for =
    non_cv_trait<Trait>                                                                                 //
    and stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>, meta::is_static_member)    //
    and stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>, meta::is_function)         //
    and stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>,
                     first_parameter_is_cvref_of<mock_trait_ref<Trait>>)    //
    ;

template<template<typename> typename DefaultImpl, typename Trait>
concept strict_default_impl_for = default_impl_for<DefaultImpl, Trait>    //
                                  and stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>,
                                                   maps_to_a_trait_method_of<Trait>)    //
    ;


template<any_trait Trait>
inline constexpr auto mandatory_trait_methods = [] {
    static constexpr meta::info default_impl_template =
        decltype(default_trait_impl_identity<std::remove_cv_t<Trait>>::template_info)::value;
    using default_trait_impl_mock = [:meta::substitute(default_impl_template,
                                                       {^^mock_trait_ref<std::remove_cv_t<Trait>>}):];
    auto methods                  = std::vector(std::from_range, all_trait_methods<Trait>);
    for (auto m: nonspecial_members<default_trait_impl_mock>    //
                     | stdv::transform(default_impl_to_method_identity)) {
        auto [s, e] = stdr::remove(methods, m);
        methods.erase(s, e);
    }
    return std::define_static_array(methods);
}();


template<meta::info Self, typename Impl, typename Trait, uZ I = 0>
concept implements_methods =
    requires { requires I == stdr::size(mandatory_trait_methods<Trait>); }                        //
    or ([:meta::substitute(^^implements_method, {^^Impl, mandatory_trait_methods<Trait>[I]}):]    //
        and
           [:meta::substitute(
                 Self, {meta::reflect_constant(Self), ^^Impl, ^^Trait, meta::reflect_constant(I + 1)}):]);

}    // namespace detail

template<typename Impl, typename Trait>
concept implements_trait = any_trait<Trait> and std::is_class_v<Impl> and
                           detail::implements_methods<^^detail::implements_methods, Impl, Trait>;
}    // namespace trp
