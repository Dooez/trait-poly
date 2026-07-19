#pragma once
#ifndef TRP_GODBOLT
#include "explicit_trait_impl.hpp"
#endif

namespace trp {
template<non_cv_trait Trait>
struct default_impl_spec {};

namespace detail {

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


template<uZ Idx, bool C, bool V, typename... Args>
struct overload_proxy {
    auto operator()(Args...) const volatile -> constant_wrapper<Idx>
        requires(C and V);
    auto operator()(Args...) const -> constant_wrapper<Idx>
        requires(C and not V);
    auto operator()(Args...) volatile -> constant_wrapper<Idx>
        requires(not C and V);
    auto operator()(Args...) -> constant_wrapper<Idx>
        requires(not C and not V);
};
template<typename... Proxies>
struct overload_tester : Proxies... {
    using Proxies::operator()...;
};
template<typename Overloaded, typename... Args>
inline constexpr auto overload_idx = std::invoke_result_t<Overloaded, Args...>::value;

template<meta::reflection_range R = std::initializer_list<meta::info>>
consteval auto resolve_method_overload_set(meta::info method_idt, R&& callable_methods) -> meta::info {
    if (stdr::empty(callable_methods))
        return meta::info{};
    if (std::size(callable_methods) == 1)
        return callable_methods[0];

    auto const params = extract_method_param_types(method_idt);
    auto const quals  = extract_method_qualifiers(method_idt);

    auto proxies = std::vector<meta::info>{};
    for (auto [i, m]: stdv::zip(stdv::iota(0), callable_methods)) {
        if (is_function_template(m)) {
            // currently cannot resolve templates
            // if the template is the only callable member, it's already handled
            return meta::info{};
        }
        auto const c_arg       = meta::reflect_constant(is_const(m));
        auto const v_arg       = meta::reflect_constant(is_volatile(m));
        auto       proxy_targs = std::vector{meta::reflect_constant(i), c_arg, v_arg};
        proxy_targs.append_range(parameters_of(m) | stdv::transform(meta::type_of));
        proxies.push_back(substitute(^^overload_proxy, proxy_targs));
    }
    auto tester = substitute(^^overload_tester, proxies);
    if (quals.is_const)
        tester = std::meta::add_const(tester);
    if (quals.is_volatile)
        tester = std::meta::add_volatile(tester);
    auto ov_idx_targs = std::vector{tester};
    ov_idx_targs.append_range(params);
    if (not extract<bool>(substitute(^^std::invocable, ov_idx_targs)))
        return meta::info{};
    auto ov_idx = extract<uZ>(substitute(^^overload_idx, ov_idx_targs));
    return callable_methods[ov_idx];
}

template<non_cvref Impl, any_method_idt MethodIdt>
inline constexpr auto matching_id_direct_public_members_for_method =
    matching_id_direct_public_members<Impl, MethodIdt::identifier>;

consteval auto find_explicit_impl(meta::info impl, meta::info ncv_trait, meta::info method_idt) -> meta::info;

template<non_ref Impl, non_cv_trait Trait, any_method_idt MethodIdt>
inline constexpr auto explicit_impl_for = find_explicit_impl(^^Impl, ^^Trait, ^^MethodIdt);

consteval auto find_explicit_impl(meta::info impl, meta::info ncv_trait, meta::info method_idt)
    -> meta::info {
    auto const nsm =
        subextract_info_span(^^nonspecial_members, {substitute(^^impl_spec_for, {impl, ncv_trait})});
    for (auto const m: nsm | stdv::filter(meta::is_template))
        if (matching_explicit_impl(explicit_impl_to_method_identity(m, ncv_trait), method_idt))
            return m;
    auto const bases = subextract_base_types(ncv_trait);
    for (auto const b: bases) {
        auto const m = extract<meta::info>(substitute(^^explicit_impl_for, {impl, b, method_idt}));
        if (m != meta::info{})
            return m;
    }
    return meta::info{};
}

/*
 * @return  nullopt if no matching names were found. 
 *          meta::info{} if member overload set could not be resolved.
 *          explicit impl, member function or function template otherwise.
*/
consteval auto find_trait_method_impl(meta::info                      impl,
                                      meta::info                      ncv_trait,
                                      meta::info                      method_idt,
                                      method_signature_requirements_t reqs) -> std::optional<meta::info> {
    auto const m =
        extract<meta::info>(substitute(^^explicit_impl_for, {remove_cv(impl), ncv_trait, method_idt}));
    if (m != meta::info{})
        return m;

    auto const id_mems =
        subextract_info_span(^^matching_id_direct_public_members_for_method, {remove_cv(impl), method_idt});
    if (id_mems.empty())
        return std::nullopt;

    auto const invk_concept = reqs.exact_return ? ^^std::same_as : ^^std::convertible_to;
    auto const callable_mems =
        id_mems |
        stdv::filter([=](auto m) { return invocable_as_method(impl, m, method_idt, invk_concept); }) |
        stdr::to<std::vector>();

    for (auto const m: callable_mems) {
        auto const matches = check_parameter_match(impl, m, method_idt, reqs.exact_cv, reqs.exact_ref);
        if (matches)
            return m;
    }

    if (not reqs.exact_args)
        if (auto m = resolve_method_overload_set(method_idt, callable_mems); m != meta::info{})
            return m;

    return meta::info{};
};

template<non_ref Impl, non_cv_trait Trait>
inline constexpr auto full_impls_for = [] {
    auto impls = std::vector<impl_method_bind>();
    // for (auto [method_idt, reqs]: all_trait_methods_and_requirements<Trait>.zip()) {
    auto const idts     = all_trait_methods_and_requirements<Trait>.identities;
    auto const all_reqs = all_trait_methods_and_requirements<Trait>.requirements;
    for (auto i: stdv::iota(0U, idts.size())) {
        auto const method_idt = idts[i];
        auto const reqs       = all_reqs[i];

        auto o_m = find_trait_method_impl(^^Impl, ^^Trait, method_idt, reqs);
        if (not o_m) {
            o_m = [=] -> std::optional<meta::info> {
                auto       checked_bases = std::vector<meta::info>{};
                auto       next_bases    = std::vector<meta::info>{};
                auto const apply_cv = stdv::transform([](auto base) { return copy_cv_to(^^Impl, base); });

                auto m              = meta::info{};
                auto base_candidate = meta::info{};

                auto bases = subextract_info_span(^^direct_base_types, {remove_cv(^^Impl)})    //
                             | apply_cv                                                        //
                             | stdr::to<std::vector<meta::info>>();
                while (not stdr::empty(bases)) {
                    for (auto const base: bases) {
                        if (base_candidate == base) {
                            //ambiguous because the fitting method found in multiple base-class subobjects
                            return meta::info{};
                        }
                        auto const o_m = find_trait_method_impl(base, ^^Trait, method_idt, reqs);
                        if (o_m) {
                            if (m != meta::info{}) {
                                // method present in multiple bases, call would be ambiguous
                                return meta::info{};
                            }
                            if (*o_m == meta::info{}) {
                                // could not resolve method for a base
                                return meta::info{};
                            }
                            m              = *o_m;
                            base_candidate = base;
                        } else {
                            checked_bases.push_back(base);
                            auto const nb = subextract_info_span(^^direct_base_types, {remove_cv(base)});
                            next_bases.append_range(
                                nb            //
                                | apply_cv    //
                                | stdv::filter([&](auto r) { return not stdr::contains(checked_bases, r); }));
                        }
                    }
                    bases = next_bases;
                    next_bases.clear();
                }
                if (m != meta::info{})
                    return m;
                return std::nullopt;
            }();
        }
        auto m = o_m.value_or(meta::info{});
        if (m == meta::info{})
            for (auto const bind: all_default_impls<Trait>)
                if (matching_explicit_impl(bind.idt, method_idt)) {
                    m = bind.fn;
                    break;
                }
        impls.emplace_back(m, method_idt);
    }
    return std::define_static_array(impls);
}();


template<typename Impl, typename Trait>
concept has_impls_for_all_methods =
    non_ref<Impl>           //
    and any_trait<Trait>    //
    and stdr::all_of(full_impls_for<Impl, std::remove_cv_t<Trait>>, [](impl_method_bind bind) {
            auto const is_relevant_method = (is_const_idt(bind.idt) or not is_const(^^Trait))    //
                                            and (is_volatile_idt(bind.idt) or not is_volatile(^^Trait));
            return not is_relevant_method or bind.fn != meta::info{};
        });
}    // namespace detail

template<typename Impl, typename Trait>
concept implements_trait =
    any_trait<Trait>                                                                                        //
    and std::is_class_v<Impl>                                                                               //
    and detail::valid_explicit_impl_spec<impl_spec_for<std::remove_cv_t<Impl>, std::remove_cv_t<Trait>>>    //
    and detail::has_impls_for_all_methods<Impl, Trait>;
}    // namespace trp
