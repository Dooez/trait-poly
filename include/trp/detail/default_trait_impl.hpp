#pragma once
#include <meta>
#ifndef TRP_GODBOLT
#include "cvtmock_trait_ref.hpp"
#endif

namespace trp {

template<non_cv_trait Trait>
struct default_impl_info {
    static constexpr meta::info value = meta::info{};
};

namespace detail {

template<typename T>
struct empty_default_implementation {};

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

template<non_cv_trait Trait, non_ref Impl>
using default_trait_impl_for = [:[] {
    if (default_impl_info<Trait>::value != meta::info{}) {
        const bool valid_specialization =
            extract<bool>(meta::substitute(^^default_impl_for, {default_impl_info<Trait>::value, ^^Trait}));
        if (not valid_specialization)
            throw "Invalid specialization of trp::default_impl_for<Trait>";
        return meta::substitute(default_impl_info<Trait>::value, {^^Impl});
    }
    auto trait_anns = meta::annotations_of(^^Trait);
    auto it         = stdr::find_if(trait_anns, is_default_impl_annotation);
    if (it != trait_anns.end())
        return substitute(meta::template_arguments_of(meta::type_of(*it))[0], {^^Impl});
    return substitute(^^empty_default_implementation, {^^Impl});
}():];

template<any_trait Trait>
inline constexpr auto mandatory_trait_methods = [] {
    using unqual_trait            = std::remove_cv_t<Trait>;
    using default_trait_impl_mock = default_trait_impl_for<unqual_trait, mock_trait_ref<unqual_trait>>;

    auto methods = std::vector(std::from_range, all_trait_methods<Trait>);
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

template<template<typename> typename DefaultImpl>
inline constexpr auto use_default_impl = detail::default_impl_annotation_t<DefaultImpl>{};

template<typename Impl, typename Trait>
concept implements_trait = any_trait<Trait> and std::is_class_v<Impl> and
                           detail::implements_methods<^^detail::implements_methods, Impl, Trait>;
}    // namespace trp
