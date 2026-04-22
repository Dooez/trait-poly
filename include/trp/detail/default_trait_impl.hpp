#pragma once
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif

namespace trp {
template<non_cv_trait Trait>
struct default_impl_spec {};

namespace detail {

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

template<typename DefaultImpl, typename Trait>
concept default_impl_for =
    non_cv_trait<Trait>                                                               //
    and stdr::all_of(nonspecial_members<DefaultImpl>, is_default_impl_fn_template)    //
    ;

template<typename DefaultImpl, typename Trait>
concept strict_default_impl_for = default_impl_for<DefaultImpl, Trait>    //
                                  and stdr::all_of(nonspecial_members<DefaultImpl>,
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

template<typename Impl, typename Trait>
concept implements_trait = any_trait<Trait> and std::is_class_v<Impl> and
                           detail::implements_methods<^^detail::implements_methods, Impl, Trait>;
}    // namespace trp
