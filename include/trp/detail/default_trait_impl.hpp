#pragma once
#ifndef TRP_GODBOLT
#include "explicit_trait_impl.hpp"
#endif

namespace trp {
template<non_cv_trait Trait>
struct default_impl_spec {};

namespace detail {
namespace concepts {
template<any_trait Trait>
consteval bool maps_to_a_trait_method_of(meta::info fn) {
    return stdr::contains(all_trait_methods<Trait>, explicit_impl_to_method_identity(fn, ^^Trait));
};

template<any_trait Trait>
consteval bool is_explicit_template_method_impl(meta::info fn) {
    return is_static_member(fn)            //
           and is_function_template(fn)    //
           and first_parameter_is_non_eop_cvref_of<mock_trait_ref<Trait>>(
                   substitute(fn, {^^mock_trait_ref<Trait>}));
}
template<meta::info Fn, typename Trait>
concept explicit_template_method_impl_of = is_explicit_template_method_impl<Trait>(Fn);
}    // namespace concepts

template<typename DefaultImpl, typename Trait>
concept default_impl_spec_for =
    non_cv_trait<Trait>                                                                                     //
    and stdr::all_of(nonspecial_members<DefaultImpl>, concepts::is_explicit_template_method_impl<Trait>)    //
    ;

template<typename DefaultImpl, typename Trait>
concept strict_default_impl_spec_for = default_impl_spec_for<DefaultImpl, Trait>    //
                                       and stdr::all_of(nonspecial_members<DefaultImpl>,
                                                        concepts::maps_to_a_trait_method_of<Trait>)    //
    ;

struct impl_method_bind {
    meta::info fn;
    meta::info idt;
};

template<non_cv_trait Trait>
inline constexpr auto all_default_impls = [] {
    auto impls = std::vector<impl_method_bind>{};
    for (auto const m: nonspecial_members<default_impl_spec<Trait>> |
                           stdv::filter(concepts::is_explicit_template_method_impl<Trait>))
        impls.emplace_back(m, explicit_impl_to_method_identity(m, ^^Trait));

    auto const append_unique = [&](auto&& method_impls) {
        for (auto const m: method_impls)
            if (not stdr::contains(impls, m.idt, &impl_method_bind::idt))
                impls.push_back(m);
    };
    append_unique(nonspecial_members<Trait>                                            //
                  | stdv::filter(concepts::is_explicit_template_method_impl<Trait>)    //
                  | stdv::transform([](auto m) {
                        return impl_method_bind{m, explicit_impl_to_method_identity(m, ^^Trait)};
                    }));
    template for (constexpr auto base: direct_base_types<Trait>) {
        using base_t = [:base:];
        append_unique(all_default_impls<base_t>);
    }
    return define_static_array(impls);
}();

template<non_cvref Impl, any_method_idt MethodIdt>
inline constexpr auto matching_id_direct_public_members_for_method =
    matching_id_direct_public_members<Impl, MethodIdt::identifier>;

consteval auto find_explicit_trait_method_impl(meta::info ncv_trait, meta::info impl, meta::info method_idt)
    -> meta::info {
    auto const nsm =
        subextract_info_span(^^nonspecial_members, {substitute(^^impl_spec_for, {impl, ncv_trait})});
    for (auto const m: nsm)
        if (explicit_impl_to_method_identity(m, ncv_trait) == method_idt)
            return m;

    auto const bases = subextract_info_span(^^direct_base_types, {ncv_trait});
    for (auto const base: bases)
        if (auto m = find_explicit_trait_method_impl(base, impl, method_idt); m != meta::info{})
            return m;
    return {};
};
consteval auto find_trait_method_impl(meta::info ncv_trait, meta::info impl, meta::info method_idt) {
    auto const m = find_explicit_trait_method_impl(ncv_trait, impl, method_idt);
    if (m != meta::info{})
        return m;
    auto const id_mems =
        subextract_info_span(^^matching_id_direct_public_members_for_method, {remove_cv(impl), method_idt});
    for (auto const m: id_mems) {
        auto matches = extract<bool>(substitute(^^strictly_matches, {impl, reflect_constant(m), method_idt}));
        if (matches)
            return m;
    }
    return meta::info{};
}

template<non_ref Impl, non_cv_trait Trait>
inline constexpr auto full_impls_for = [] {
    auto impls = std::vector<impl_method_bind>();
    for (auto method_idt: all_trait_methods<Trait>) {
        auto m = find_trait_method_impl(^^Trait, ^^Impl, method_idt);
        if (m == meta::info{}) {
            m = [=] {
                auto       checked_bases = std::vector<meta::info>{};
                auto       next_bases    = std::vector<meta::info>{};
                auto const apply_cv = stdv::transform([](auto base) { return copy_cv_to(^^Impl, base); });

                auto bases = subextract_info_span(^^direct_base_types, {remove_cv(^^Impl)})    //
                             | apply_cv                                                        //
                             | stdr::to<std::vector<meta::info>>();
                while (not stdr::empty(bases)) {
                    for (auto const base: bases) {
                        if (auto m = find_trait_method_impl(^^Trait, base, method_idt); m != meta::info{})
                            return m;
                        checked_bases.push_back(base);
                        auto nb = subextract_info_span(^^direct_base_types, {remove_cv(base)});
                        next_bases.append_range(
                            nb            //
                            | apply_cv    //
                            | stdv::filter([&](auto r) { return not stdr::contains(checked_bases, r); }));
                    }
                    bases = next_bases;
                    next_bases.clear();
                }
                return meta::info{};
            }();
        }
        if (m == meta::info{}) {
            for (auto const bind: all_default_impls<Trait>)
                if (bind.idt == method_idt) {
                    m = bind.fn;
                    break;
                }
        }
        impls.emplace_back(m, method_idt);
    }
    return std::define_static_array(impls);
}();


template<non_ref Impl, any_trait Trait>
inline constexpr auto has_impls_for_all_methods =
    stdr::all_of(full_impls_for<Impl, std::remove_cv_t<Trait>>, [](impl_method_bind bind) {
        auto const is_relevant_method = (is_const_idt(bind.idt) or not is_const(^^Trait))    //
                                        and (is_volatile_idt(bind.idt) or not is_volatile(^^Trait));
        return not is_relevant_method or bind.fn != meta::info{};
    });
}    // namespace detail

template<typename Impl, typename Trait>
concept implements_trait =
    any_trait<Trait> and std::is_class_v<Impl> and detail::has_impls_for_all_methods<Impl, Trait>;
}    // namespace trp
