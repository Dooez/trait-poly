#pragma once
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
namespace trp {
template<non_cvref Impl, non_cv_trait Trait>
struct impl_spec_for {};

namespace detail {
template<typename T>
struct trait_of_impl_spec;

template<non_ref Impl, non_cv_trait Trait>
struct trait_of_impl_spec<impl_spec_for<Impl, Trait>> {
    using type = Trait;
};
//
namespace concepts {
template<typename T>
concept only_static_members = stdr::all_of(nonspecial_members<T>, meta::is_static_member);

template<any_trait Trait>
consteval bool maps_to_a_trait_method_of(meta::info fn) {
    return stdr::contains(all_trait_methods<Trait>, explicit_impl_to_method_identity(fn, ^^Trait));
};

consteval bool is_explicit_template_method_impl(meta::info fn, meta::info trait) {
    return is_static_member(fn)            //
           and is_function_template(fn)    //
           and first_parameter_is_non_eop_cvref_of(substitute(fn, {substitute(^^mock_trait_ref, {trait})}),
                                                   substitute(^^mock_trait_ref, {trait}));
}
template<meta::info Fn>
concept any_static_member_info = is_static_member(Fn);
template<meta::info Fn>
concept any_fn_template_info = is_function_template(Fn);
}    // namespace concepts

template<typename ExplicitImpl, typename Trait>
concept explicit_impl_spec_for = non_cv_trait<Trait>                                                       //
                                 and concepts::no_private_members<ExplicitImpl>                            //
                                 and concepts::no_private_bases<ExplicitImpl>                              //
                                 and concepts::no_virtual<ExplicitImpl>                                    //
                                 and concepts::no_explicit_special_members<ExplicitImpl>                   //
                                 and concepts::only_static_members<ExplicitImpl>                           //
                                 and concepts::static_methods_are_default_impl_for<ExplicitImpl, Trait>    //
    ;

template<typename T>
concept valid_explicit_impl_spec = explicit_impl_spec_for<T, typename trait_of_impl_spec<T>::type>;

}    // namespace detail
}    // namespace trp
