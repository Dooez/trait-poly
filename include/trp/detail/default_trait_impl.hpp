#pragma once
#ifndef TRP_GODBOLT
#include "explicit_trait_impl.hpp"
#endif

namespace trp {
template<non_cv_trait Trait>
struct default_impl_spec {};

namespace detail {

template<any_trait Trait>
consteval bool maps_to_a_trait_method_of(meta::info fn) {
    return stdr::contains(all_trait_methods<Trait>, explicit_impl_to_method_identity(fn));
};

template<any_trait Trait>
consteval bool is_explicit_method_impl_template(meta::info fn) {
    return is_static_member(fn)            //
           and is_function_template(fn)    //
           and first_parameter_is_non_eop_cvref_of<mock_trait_ref<Trait>>(
                   substitute(fn, {^^mock_trait_ref<Trait>}));
}
template<typename Impl>
consteval bool is_explicit_method_impl(meta::info fn) {
    return is_static_member(fn)    //
           and is_function(fn)     //
           and first_parameter_is_non_eop_cvref_of<Impl>(fn);
}

template<typename DefaultImpl, typename Trait>
concept default_impl_for =
    non_cv_trait<Trait>                                                                    //
    and stdr::all_of(nonspecial_members<DefaultImpl>, is_explicit_method_impl_template)    //
    ;

template<typename DefaultImpl, typename Trait>
concept strict_default_impl_for = default_impl_for<DefaultImpl, Trait>    //
                                  and stdr::all_of(nonspecial_members<DefaultImpl>,
                                                   maps_to_a_trait_method_of<Trait>)    //
    ;


struct impl_method_bind {
    meta::info fn;
    meta::info idt;
};

template<non_cv_trait Trait>
inline constexpr auto all_default_impls = [] {
    using namespace meta;
    auto impls = std::vector<impl_method_bind>{};

    for (auto m:
         nonspecial_members<default_impl_spec<Trait>> | stdv::filter(is_explicit_method_impl_template<Trait>))
        impls.emplace_back(m, explicit_impl_to_method_identity(m));

    const auto append_unique = [&](auto&& method_impls) {
        for (auto m: method_impls)
            if (not stdr::contains(impls, m.idt, &impl_method_bind::idt))
                impls.push_back(m);
    };
    append_unique(
        nonspecial_members<Trait>                                  //
        | stdv::filter(is_explicit_method_impl_template<Trait>)    //
        | stdv::transform([](auto m) { return impl_method_bind{m, explicit_impl_to_method_identity(m)}; }));
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


template<non_cvref Impl, any_method_idt MethodIdt>
inline constexpr auto matching_id_direct_public_members_for_method =
    matching_id_direct_public_members<Impl, MethodIdt::identifier>;


consteval auto search_trait_specialization(meta::info trait, meta::info impl, meta::info method_idt)
    -> meta::info {
    using namespace meta;
    auto nsm = extract<std::span<const info>>(
        substitute(^^nonspecial_members, {substitute(^^impl_spec_for, {impl, trait})}));
    for (auto m: nsm)
        if (explicit_impl_to_method_identity(m) == method_idt)
            return m;

    auto bases = extract<std::span<const info>>(substitute(^^direct_base_types, {trait}));
    for (auto base: bases)
        if (auto m = search_trait_specialization(copy_cv_to(trait, base), impl, method_idt); m != info{})
            return m;

    auto id_mems = extract<std::span<const info>>(
        substitute(^^matching_id_direct_public_members_for_method, {remove_cv(impl), method_idt}));
    for (auto m: id_mems) {
        auto matches = extract<bool>(substitute(^^strictly_matches, {impl, reflect_constant(m), method_idt}));
        if (matches)
            return m;
    }
    return info{};
};

template<non_cvref Impl, non_cv_trait Trait>
inline constexpr auto full_impls_for = [] {
    using namespace meta;

    auto impls = std::vector<impl_method_bind>();
    for (auto method_idt: all_trait_methods<Trait>) {
        auto m = search_trait_specialization(^^Trait, ^^Impl, method_idt);
        if (m == meta::info{}) {
            m = [=] {
                auto checked_bases = std::vector<meta::info>{};
                auto next_bases    = std::vector<meta::info>{};
                auto bases = extract<std::span<const info>>(substitute(^^direct_base_types, {^^Impl})) |
                             stdr::to<std::vector<info>>();
                while (not stdr::empty(bases)) {
                    for (auto base: bases) {
                        auto m = search_trait_specialization(^^Trait, base, method_idt);
                        if (m != info{})
                            return m;
                        checked_bases.push_back(base);
                        auto nb = extract<std::span<const info>>(substitute(^^direct_base_types, {base}));
                        next_bases.append_range(
                            nb | stdv::filter([&](auto r) { return not stdr::contains(checked_bases, r); }));
                    }
                    bases = next_bases;
                    next_bases.clear();
                }
                return meta::info{};
            }();
        }
        if (m == meta::info{}) {
            for (auto bind: all_default_impls<Trait>)
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
inline constexpr auto impls_for = [] {
    constexpr auto is_matching_method = [](impl_method_bind bind) {
        return (is_const_idt(bind.idt) or not is_const(^^Trait))    //
               and (is_volatile_idt(bind.idt) or not is_volatile(^^Trait));
    };
    constexpr auto is_invokable_method = [](impl_method_bind bind) {
        return (is_const_idt(bind.idt) or not is_const(^^Impl))    //
               and (is_volatile_idt(bind.idt) or not is_volatile(^^Impl));
    };
    return std::define_static_array(full_impls_for<std::remove_cv_t<Impl>, std::remove_cv_t<Trait>>    //
                                    | stdv::filter(is_matching_method)                                 //
                                    | stdv::transform([=](auto bind) {
                                          if (not is_invokable_method(bind))
                                              bind.fn = {};
                                          return bind;
                                      }));
}();

}    // namespace detail

template<typename Impl, typename Trait>
concept implements_trait =
    any_trait<Trait> and std::is_class_v<Impl> and
    stdr::none_of(detail::impls_for<Impl, Trait>, [](auto bind) { return bind.fn == meta::info{}; });
}    // namespace trp
