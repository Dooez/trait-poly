#pragma once
#ifndef TRP_GODBOLT
#include "alias_and_helpers.hpp"
#endif

namespace trp::detail {
namespace cvtmock_trait {

template<trait_method_idt>
struct cvtmock_cvo_invoker;

template<auto Identifier,
         bool Const,
         bool Volatile,
         bool LVRef,
         bool RVRef,
         bool Value,
         bool Noexcept,
         typename Ret,
         typename... Args>
    requires(not(LVRef or RVRef or Value))    // not supported
struct cvtmock_cvo_invoker<
    method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>> {
    auto operator()(Args...) noexcept(Noexcept) -> Ret
        requires(not Const and not Volatile)
    {
        std::unreachable();
    }
    auto operator()(Args...) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {
        std::unreachable();
    }
    auto operator()(Args...) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {
        std::unreachable();
    }
    auto operator()(Args...) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {
        std::unreachable();
    }
};

template<trait_method_idt... MethodIds>
struct cvtmock_cvm_invoker : cvtmock_cvo_invoker<MethodIds>... {
    using cvtmock_cvo_invoker<MethodIds>::operator()...;
};
template<const char* id, typename Invoker>
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

template<const char* id, typename Invoker>
using cvtmock_holder = typename cvtmock_holder_definer<id, Invoker>::cvtmock_method_holder;

template<typename... MethodHolders>
struct mock_ref_impl : public MethodHolders... {};

template<non_cvref Trait>
consteval auto get_cvtmock_ref() {
    struct method_holder_spec {
        const char*             id;
        std::vector<meta::info> method_idts;
    };
    auto method_holders_specs = std::vector<method_holder_spec>{};

    template for (constexpr auto mem: all_trait_methods<Trait>) {
        using method_idt = [:mem:];
        auto it          = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
        if (it == method_holders_specs.end()) {
            method_holders_specs.push_back({.id          = method_idt::identifier,    //
                                            .method_idts = {mem}});
        } else {
            it->method_idts.push_back(mem);
        }
    }
    return substitute(^^mock_ref_impl,
                      method_holders_specs    //
                          | stdv::transform([](auto spec) {
                                return substitute(^^cvtmock_holder,
                                                  {meta::reflect_constant(spec.id),
                                                   substitute(^^cvtmock_cvm_invoker, spec.method_idts)});
                            }));
};
}    // namespace cvtmock_trait

// cv-transient trait reference mock
template<non_cvref Trait>
using mock_trait_ref = [:cvtmock_trait::get_cvtmock_ref<Trait>():];
}    // namespace trp::detail
