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
    and stdr::all_of(nonspecial_members<T>, first_parameter_is_cvref_of<Impl>);

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
//
// struct impl_method_bind {
//     meta::info fn;
//     meta::info idt;
// };
//
// template<non_cvref Impl, any_method_idt MethodIdt>
// inline constexpr auto matching_id_direct_public_members_for_method =
//     matching_id_direct_public_members<Impl, MethodIdt::identifier>;
//
// template<non_cvref Impl, non_cv_trait Trait>
// inline constexpr auto full_impls_for = [] {
//     using namespace meta;
//     constexpr auto search_in_trait_specializations =
//         [](this auto self, info trait, info impl, info method_idt) -> info {
//         auto nsm = extract<std::span<const info>>(
//             substitute(^^nonspecial_members, {substitute(^^impl_spec_for, {impl, trait})}));
//         for (auto m: nsm)
//             if (explicit_impl_to_method_identity(m) == method_idt)
//                 return m;
//
//         auto bases = extract<std::span<const info>>(substitute(^^direct_base_types, {trait}));
//         for (auto base: bases)
//             if (auto m = self(copy_cv_to(trait, base), impl, method_idt); m != info{})
//                 return m;
//
//         auto id_mems = extract<std::span<const info>>(
//             substitute(^^matching_id_direct_public_members_for_method, {remove_cv(impl), method_idt}));
//         for (auto m: id_mems) {
//             auto matches = extract<bool>(substitute(^^strictly_matches, {impl, m, method_idt}));
//             if (matches)
//                 return m;
//         }
//         return info{};
//     };
//
//     auto impls = std::vector<impl_method_bind>();
//     for (auto method_idt: direct_trait_methods<Trait>) {
//         auto m = search_in_trait_specializations(^^Trait, ^^Impl, method_idt);
//         if (m == meta::info{}) {
//             m = [=] {
//                 auto checked_bases = std::vector<meta::info>{};
//                 auto next_bases    = std::vector<meta::info>{};
//                 auto bases = extract<std::span<const info>>(substitute(^^direct_base_types, {^^Impl})) |
//                              stdr::to<std::vector<info>>();
//                 while (not stdr::empty(bases)) {
//                     for (auto base: bases) {
//                         auto m = search_in_trait_specializations(^^Trait, base, method_idt);
//                         if (m != info{})
//                             return m;
//                         checked_bases.push_back(base);
//                         auto nb = extract<std::span<const info>>(substitute(^^direct_base_types, {base}));
//                         next_bases.append_range(
//                             nb | stdv::filter([&](auto r) { return not stdr::contains(checked_bases, r); }));
//                     }
//                     bases = next_bases;
//                     next_bases.clear();
//                 }
//             }();
//         }
//         impls.emplace_back(m, method_idt);
//     }
//     return std::define_static_array(impls);
// }();
//
//
// template<non_ref Impl, any_trait Trait>
// inline constexpr auto impls_for = [] {
//     constexpr auto is_matching_method = [](impl_method_bind bind) static {
//         return (is_const_idt(bind.idt) or not is_const(^^Trait))    //
//                and (is_volatile_idt(bind.idt) or not is_volatile(^^Trait));
//     };
//     return std::define_static_array(full_impls_for<std::remove_cv_t<Impl>, std::remove_cv_t<Trait>>    //
//                                     | stdv::filter(is_matching_method));
// }();
//
// template<typename Impl, typename Trait, typename MethodIdt>
// concept implements_method = non_ref<Impl>                      //
//                             and any_trait<Trait>               //
//                             and trait_method_idt<MethodIdt>    //
//                             and (matching_impl_method<Impl, MethodIdt> != meta::info{});
//
// template<meta::info Self, typename Impl, typename Trait, uZ I = 0>
// concept implements_methods =
//     requires { requires I == stdr::size(mandatory_trait_methods<Trait>); }                        //
//     or ([:meta::substitute(^^implements_method, {^^Impl, mandatory_trait_methods<Trait>[I]}):]    //
//         and
//            [:meta::substitute(
//                  Self, {meta::reflect_constant(Self), ^^Impl, ^^Trait, meta::reflect_constant(I + 1)}):]);
//
}    // namespace detail
}    // namespace trp
