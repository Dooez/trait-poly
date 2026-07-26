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
    bool       is_explicit;
};

struct bind_transformer {
    meta::info     trait;
    consteval auto operator()(meta::info m) const -> impl_method_bind {
        return impl_method_bind{m, explicit_impl_to_method_identity(m, trait)};
    }
};

consteval auto all_default_impls_fn(meta::info trait) -> std::span<impl_method_bind const>;

template<non_cv_trait Trait>
inline constexpr auto all_default_impls = all_default_impls_fn(^^Trait);

consteval auto all_default_impls_fn(meta::info trait) -> std::span<impl_method_bind const> {
    auto       impls = std::vector<impl_method_bind>{};
    auto const nsm_spec =
        subextract_info_span(^^nonspecial_members, {substitute(^^default_impl_spec, {trait})});
    for (auto const m: nsm_spec)
        if (concepts::is_explicit_template_method_impl(m, trait))
            impls.emplace_back(m, explicit_impl_to_method_identity(m, trait), true);

    auto const nsm_inline = subextract_info_span(^^nonspecial_members, {trait});
    for (auto const m: nsm_inline)
        if (concepts::is_explicit_template_method_impl(m, trait))
            impls.emplace_back(m, explicit_impl_to_method_identity(m, trait), true);

    for (auto base: subextract_base_types(trait)) {
        auto const base_impls = subextract_span<impl_method_bind>(^^all_default_impls, {base});
        for (auto const m: base_impls)
            if (not stdr::contains(impls, m.idt, &impl_method_bind::idt))
                impls.push_back(m);
    }
    return define_static_array(impls);
}


template<uZ Idx, bool C, bool V, bool L, bool R, bool Static, typename... Args>
struct overload_proxy {
    auto operator()(Args...) const volatile -> constant_wrapper<Idx>
        requires(not Static and C and V and not L and not R);
    auto operator()(Args...) const -> constant_wrapper<Idx>
        requires(not Static and C and not V and not L and not R);
    auto operator()(Args...) volatile -> constant_wrapper<Idx>
        requires(not Static and not C and V and not L and not R);
    auto operator()(Args...) -> constant_wrapper<Idx>
        requires(not Static and not C and not V and not L and not R);

    auto operator()(Args...) const volatile& -> constant_wrapper<Idx>
        requires(not Static and C and V and L);
    auto operator()(Args...) const& -> constant_wrapper<Idx>
        requires(not Static and C and not V and L);
    auto operator()(Args...) volatile& -> constant_wrapper<Idx>
        requires(not Static and not C and V and L);
    auto operator()(Args...) & -> constant_wrapper<Idx>
        requires(not Static and not C and not V and L);

    auto operator()(Args...) const volatile&& -> constant_wrapper<Idx>
        requires(not Static and C and V and R);
    auto operator()(Args...) const&& -> constant_wrapper<Idx>
        requires(not Static and C and not V and R);
    auto operator()(Args...) volatile&& -> constant_wrapper<Idx>
        requires(not Static and not C and V and R);
    auto operator()(Args...) && -> constant_wrapper<Idx>
        requires(not Static and not C and not V and R);

    static auto operator()(Args...) -> constant_wrapper<Idx>
        requires(Static);
};
template<typename... Proxies>
struct overload_tester : Proxies... {
    using Proxies::operator()...;
};
template<typename Overloaded, typename... Args>
inline constexpr auto overload_idx = std::invoke_result_t<Overloaded, Args...>::value;

consteval auto resolve_method_overload_set(meta::info                  method_idt,
                                           std::span<meta::info const> callable_methods) -> meta::info {
    if (callable_methods.empty())
        return meta::info{};
    if (callable_methods.size() == 1)
        return callable_methods[0];

    auto const params = extract_method_param_types(method_idt);
    auto const quals  = extract_method_qualifiers(method_idt);

    auto proxies = std::vector<meta::info>{};
    for (uZ i = 0; auto m: callable_methods) {
        if (is_function_template(m)) {
            // currently cannot resolve templates
            // if the template is the only callable member, it's already handled
            return meta::info{};
        }
        auto const raw_params  = parameters_of(m);
        auto const is_eop      = not raw_params.empty() and is_explicit_object_parameter(raw_params[0]);
        auto const object_type = is_eop ? type_of(raw_params[0]) : meta::info{};
        auto const c_arg = meta::reflect_constant(is_eop ? meta::is_const(remove_reference(object_type))    //
                                                         : is_const(m));
        auto const v_arg = meta::reflect_constant(is_eop ? meta::is_volatile(remove_reference(object_type))
                                                         : is_volatile(m));
        auto const l_arg = meta::reflect_constant(is_eop ? is_lvalue_reference_type(object_type)
                                                         : is_lvalue_reference_qualified(m));
        auto const r_arg = meta::reflect_constant(is_eop ? is_rvalue_reference_type(object_type)
                                                         : is_rvalue_reference_qualified(m));
        auto const static_arg = meta::reflect_constant(is_static_member(m));
        auto proxy_targs = std::vector{meta::reflect_constant(i), c_arg, v_arg, l_arg, r_arg, static_arg};
        auto const max_param_idx = std::min(params.size(), raw_params.size() - (is_eop ? 1 : 0));
        for (auto i: stdv::iota(0U, max_param_idx))
            proxy_targs.push_back(type_of(raw_params[i + (is_eop ? 1 : 0)]));
        proxies.push_back(substitute(^^overload_proxy, proxy_targs));
        ++i;
    }
    auto tester = substitute(^^overload_tester, proxies);
    if (quals.is_const)
        tester = std::meta::add_const(tester);
    if (quals.is_volatile)
        tester = std::meta::add_volatile(tester);
    if (quals.is_rvalue)
        tester = std::meta::add_rvalue_reference(tester);
    else
        tester = std::meta::add_lvalue_reference(tester);
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
    for (auto const m: nsm)
        if (is_template(m)    //
            and matching_explicit_impl(explicit_impl_to_method_identity(m, ncv_trait), method_idt))
            return m;
    auto const bases = subextract_base_types(ncv_trait);
    for (auto const b: bases) {
        auto const m = extract<meta::info>(substitute(^^explicit_impl_for, {impl, b, method_idt}));
        if (m != meta::info{})
            return m;
    }
    return meta::info{};
}
consteval auto find_explicit_trait_method_impl(meta::info impl, meta::info ncv_trait, meta::info method_idt)
    -> meta::info {
    return extract<meta::info>(substitute(^^explicit_impl_for, {remove_cv(impl), ncv_trait, method_idt}));
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
    auto const id_mems =
        subextract_info_span(^^matching_id_direct_public_members_for_method, {remove_cv(impl), method_idt});
    if (id_mems.empty())
        return std::nullopt;

    auto callable_mems = std::vector<meta::info>();
    for (auto m: id_mems)
        if (invocable_as_method(impl, m, method_idt, reqs.exact_return))
            callable_mems.push_back(m);

    // always try to find exactly matching arguments because templates cannot be resolved otherwise
    auto matching_mems     = std::vector<meta::info>();
    auto matching_template = meta::info{};

    for (auto m: callable_mems) {
        if (not is_function(m) and not is_function_template(m)) {
            auto const method_obj_t    = type_of(m);
            auto const call_ops        = subextract_info_span(^^call_operators_of, {method_obj_t});
            auto const method_inv_type = copy_cv_to(impl, method_obj_t);
            for (auto op: call_ops) {
                auto const match = check_parameter_match(method_inv_type, op, method_idt);
                if (match.satisfies(reqs))
                    return m;
            }
            continue;
        }
        auto const match = check_parameter_match(impl, m, method_idt);
        if (match.full_match()) {
            if (is_function_template(m))
                matching_template = m;
            else
                return m;
        }
        if (match.satisfies(reqs))
            matching_mems.push_back(m);
    }
    if (matching_template != meta::info{})
        return matching_template;

    // after cvref promotion multiple matching arguments can be present, need to resolve
    if (auto m = resolve_method_overload_set(method_idt, matching_mems); m != meta::info{})
        return m;

    if (not reqs.exact_args)
        if (auto m = resolve_method_overload_set(method_idt, callable_mems); m != meta::info{})
            return m;

    return meta::info{};
};


consteval auto full_impls_for_fn(meta::info impl, meta::info trait) -> std::span<impl_method_bind const>;

template<non_ref Impl, non_cv_trait Trait>
inline constexpr auto full_impls_for = full_impls_for_fn(^^Impl, ^^Trait);

consteval auto full_impls_for_fn(meta::info impl, meta::info trait) -> std::span<impl_method_bind const> {
    auto       impls    = std::vector<impl_method_bind>();
    auto const idts     = subextract_info_span(^^all_trait_methods, {trait});
    auto const all_reqs = subextract_span<method_signature_requirements_t>(^^all_trait_requirements, {trait});
    for (auto i: stdv::iota(0U, idts.size())) {
        auto const method_idt = idts[i];
        auto const reqs       = all_reqs[i];

        if (auto m = find_explicit_trait_method_impl(impl, trait, method_idt); m != meta::info{}) {
            impls.emplace_back(m, method_idt, true);
            continue;
        }
        bool is_explicit = false;
        auto o_m         = find_trait_method_impl(impl, trait, method_idt, reqs);
        if (not o_m) {
            o_m = [&] -> std::optional<meta::info> {
                auto checked_bases = std::vector<meta::info>{};
                auto next_bases    = std::vector<meta::info>{};

                auto m                  = meta::info{};
                auto base_candidate     = meta::info{};
                auto explicit_candidate = false;

                auto bases = std::vector<meta::info>{};
                for (auto base: subextract_base_types(remove_cv(impl)))
                    bases.push_back(copy_cv_to(impl, base));
                while (not stdr::empty(bases)) {
                    for (auto const base: bases) {
                        if (base_candidate == base) {
                            // handle virtual inheritance
                            auto const unambiguous_base =
                                substitute(^^std::derived_from, {remove_cv(impl), remove_cv(base)});
                            if (extract<bool>(unambiguous_base))
                                continue;
                            // ambiguous
                            return meta::info{};
                        }
                        if (auto em = find_explicit_trait_method_impl(base, trait, method_idt);
                            em != meta::info{}) {
                            if (m != meta::info{})    // multiple matches, call is ambiguous
                                return meta::info{};

                            m                  = em;
                            base_candidate     = base;
                            explicit_candidate = true;
                            continue;
                        } else if (auto const o_m = find_trait_method_impl(base, trait, method_idt, reqs)) {
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
                            continue;
                        }

                        checked_bases.push_back(base);

                        for (auto base: subextract_base_types(remove_cv(base)))
                            if (not stdr::contains(checked_bases, base))
                                next_bases.push_back(copy_cv_to(impl, base));
                    }
                    bases = next_bases;
                    next_bases.clear();
                }
                if (m != meta::info{}) {
                    is_explicit = explicit_candidate;
                    return m;
                }
                return std::nullopt;
            }();
        }
        auto m = o_m.value_or(meta::info{});
        if (m == meta::info{})
            for (auto const bind: subextract_span<impl_method_bind>(^^all_default_impls, {trait}))
                if (matching_explicit_impl(bind.idt, method_idt)) {
                    m           = bind.fn;
                    is_explicit = true;
                    break;
                }
        impls.emplace_back(m, method_idt, is_explicit);
    }
    return std::define_static_array(impls);
}

struct valid_impl_bind_for {
    meta::info     trait;
    consteval bool operator()(impl_method_bind bind) {
        auto const is_relevant_method = (is_const_idt(bind.idt) or not is_const(trait))    //
                                        and (is_volatile_idt(bind.idt) or not is_volatile(trait));
        return not is_relevant_method or bind.fn != meta::info{};
    }
};

template<typename Impl, typename Trait>
concept has_impls_for_all_methods =
    non_ref<Impl>           //
    and any_trait<Trait>    //
    and stdr::all_of(full_impls_for<Impl, std::remove_cv_t<Trait>>, valid_impl_bind_for{^^Trait});
}    // namespace detail

template<typename Impl, typename Trait>
concept implements_trait =
    any_trait<Trait>                                                                                        //
    and std::is_class_v<Impl>                                                                               //
    and detail::valid_explicit_impl_spec<impl_spec_for<std::remove_cv_t<Impl>, std::remove_cv_t<Trait>>>    //
    and detail::has_impls_for_all_methods<Impl, Trait>;
}    // namespace trp
