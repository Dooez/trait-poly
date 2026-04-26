#pragma once
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
namespace trp {
template<non_ref Impl, non_cv_trait Trait>
struct impl_spec_for {};

namespace detail {
//
namespace concepts {
template<typename T>
concept only_static_members = stdr::all_of(nonspecial_members<T>, meta::is_static_member);

template<typename T, typename Impl>
concept static_methods_are_default_impl_of =
    non_cvref<T>           //
    and non_cvref<Impl>    //
    and stdr::all_of(nonspecial_members<T>, first_parameter_is_non_eop_cvref_of<Impl>);

}    // namespace concepts
template<typename ImplSpec, typename Impl>
concept any_impl_spec_of = non_cvref<ImplSpec>                                    //
                           and non_cvref<Impl>                                    //
                           and concepts::no_explicit_special_members<ImplSpec>    //
                           and concepts::no_nonstatic_data_members<ImplSpec>      //
                           and concepts::only_static_members<ImplSpec>            //
                           and concepts::static_methods_are_default_impl_of<ImplSpec, Impl>;

template<typename ImplSpec, typename Impl, typename Trait>
concept strict_impl_spec_for = any_impl_spec_of<ImplSpec, Impl>    //
                               and concepts::static_methods_are_default_impl_for<ImplSpec, Trait>;

}    // namespace detail
}    // namespace trp
