#pragma once
#ifndef TRP_GODBOLT
#include "alias_and_helpers.hpp"
#endif

namespace trp::detail {
namespace cvtmock_trait {

template<trait_method_idt>
struct cvtmock_cvo_invoker;

template<auto Identifier, method_qualifiers_t Quals, typename Ret, typename... Args>
struct cvtmock_cvo_invoker<method_identity_t<Identifier, Quals, Ret, Args...>> {
    auto operator()(Args...) noexcept(Quals.is_noexcept) -> Ret
        requires(not Quals.is_const and not Quals.is_volatile)
    {
        std::unreachable();
    }
    auto operator()(Args...) const noexcept(Quals.is_noexcept) -> Ret
        requires(Quals.is_const and not Quals.is_volatile)
    {
        std::unreachable();
    }
    auto operator()(Args...) volatile noexcept(Quals.is_noexcept) -> Ret
        requires(not Quals.is_const and Quals.is_volatile)
    {
        std::unreachable();
    }
    auto operator()(Args...) const volatile noexcept(Quals.is_noexcept) -> Ret
        requires(Quals.is_const and Quals.is_volatile)
    {
        std::unreachable();
    }
};

template<trait_method_idt... MethodIds>
struct cvtmock_cvm_invoker : cvtmock_cvo_invoker<MethodIds>... {
    using cvtmock_cvo_invoker<MethodIds>::operator()...;
};
template<char const* id, typename Invoker>
struct cvtmock_holder_definer {
    struct cvtmock_method_holder;
    consteval {
        define_aggregate(^^cvtmock_method_holder,
                         {data_member_spec(^^Invoker,
                                           {
                                               .name              = id,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};

template<char const* id, typename Invoker>
using cvtmock_holder = typename cvtmock_holder_definer<id, Invoker>::cvtmock_method_holder;

template<typename... MethodHolders>
struct mock_ref_impl : public MethodHolders... {};

consteval auto get_holder(method_reference method_group, std::span<meta::info const> methods) -> meta::info {
    return substitute(^^cvtmock_holder,
                      {meta::reflect_constant(method_group.name),
                       substitute(^^cvtmock_cvm_invoker,
                                  methods                          //
                                      | stdv::take(method_group.end_idx)    //
                                      | stdv::drop(method_group.begin_idx))});
}

consteval auto get_cvtmock_ref(meta::info trait) -> meta::info {
    expect(^^non_cvref, {trait});
    auto const methods = subextract_info_span(^^all_trait_methods, {trait});
    auto       holders = std::vector<meta::info>();
    for (auto const grp: subextract_span<method_reference>(^^trait_method_groups, {trait})) {
        holders.push_back(get_holder(grp, methods));
    }
    return substitute(^^mock_ref_impl, holders);
};
}    // namespace cvtmock_trait

// cv-transient trait reference mock
template<non_cvref Trait>
using mock_trait_ref = [:cvtmock_trait::get_cvtmock_ref(^^Trait):];
}    // namespace trp::detail
