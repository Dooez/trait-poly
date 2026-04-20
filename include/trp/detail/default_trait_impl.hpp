#pragma once
#include <algorithm>
#include <meta>
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif

namespace trp {
template<non_cv_trait Trait>
struct default_impl_spec {};

template<non_cv_trait Trait>
struct default_impl_info {
    static constexpr meta::info value = meta::info{};
};

namespace detail {

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

template<any_trait Trait>
consteval bool is_default_impl_fn_template(meta::info fn) {
    return is_static_member(fn)            //
           and is_function_template(fn)    //
           and first_parameter_is_cvref_of<mock_trait_ref<Trait>>(substitute(fn, {^^mock_trait_ref<Trait>}));
}

template<template<typename> typename DefaultImpl, typename Trait>
concept default_impl_for =
    non_cv_trait<Trait>    //
    and
    stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>, is_default_impl_fn_template)    //
    and stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>, meta::is_function)          //
    and stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>,
                     first_parameter_is_cvref_of<mock_trait_ref<Trait>>)    //
    ;

template<template<typename> typename DefaultImpl, typename Trait>
concept strict_default_impl_for = default_impl_for<DefaultImpl, Trait>    //
                                  and stdr::all_of(nonspecial_members<DefaultImpl<mock_trait_ref<Trait>>>,
                                                   maps_to_a_trait_method_of<Trait>)    //
    ;


struct default_impl_method {
    meta::info fn;
    meta::info idt;
};

template<non_cv_trait Trait>
inline constexpr auto all_default_impls = [] {
    using namespace meta;
    auto impls = std::vector<default_impl_method>{};

    for (auto m:
         nonspecial_members<default_impl_spec<Trait>> | stdv::filter(is_default_impl_fn_template<Trait>))
        impls.emplace_back(m, default_impl_to_method_identity(m));

    const auto append_unique = [&](auto&& method_impls) {
        for (auto m: method_impls)
            if (not stdr::contains(impls, m.idt, &default_impl_method::idt))
                impls.push_back(m);
    };
    append_unique(
        nonspecial_members<Trait>                             //
        | stdv::filter(is_default_impl_fn_template<Trait>)    //
        | stdv::transform([](auto m) { return default_impl_method{m, default_impl_to_method_identity(m)}; }));
    template for (constexpr auto base: direct_base_types<Trait>) {
        using base_t = [:base:];
        append_unique(all_default_impls<base_t>);
    }
    return std::define_static_array(impls);
}();


template<non_cv_trait Trait, non_ref Impl>
using default_trait_impl_for = [:[] {
    // array<info>
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
    auto methods = std::vector(std::from_range, all_trait_methods<Trait>);
    for (auto [_, idt]: all_default_impls<std::remove_cv_t<Trait>>) {
        auto [s, e] = stdr::remove(methods, idt);
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
