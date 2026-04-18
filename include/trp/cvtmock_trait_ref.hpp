#pragma once
#include <meta>
#include <ranges>
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
#include <algorithm>
#include <type_traits>

namespace trp::detail {
namespace cvtmock_trait {

template<any_method_idt>
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
    auto operator()(Args... args) noexcept(Noexcept) -> Ret
        requires(not Const and not Volatile)
    {}
    auto operator()(Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {}
    auto operator()(Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {}
    auto operator()(Args... args) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {}
};

template<any_method_idt... MethodIds>
struct cvtmock_cvm_invoker : cvtmock_cvo_invoker<MethodIds>... {
    using cvtmock_cvo_invoker<MethodIds>::operator()...;
};

template<auto HolderSpec>
consteval auto get_cvtmock_holder() {
    struct cvtmock_method_holder;
    consteval {
        meta::define_aggregate(^^cvtmock_method_holder,
                               {meta::data_member_spec(HolderSpec.invoker_info,
                                                       meta::data_member_options{
                                                           .name              = HolderSpec.id,
                                                           .no_unique_address = true,
                                                           //  .attributes = {^^[[no_unique_address]] },
                                                       })});
    }
    return ^^cvtmock_method_holder;
}

template<typename... MethodHolders>
struct mock_ref_impl : public MethodHolders... {};

template<non_cv_trait Trait>
consteval auto define_cvtmock_ref() {
    using namespace std;
    using namespace meta;

    static constexpr auto method_holders_specs = [] {
        struct method_holder_spec {
            const char*  id;
            vector<info> method_idts;
        };
        auto method_holders_specs = vector<method_holder_spec>{};

        template for (constexpr auto mem: all_trait_methods<Trait>) {
            using method_idt = [:mem:];
            auto it = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
            if (it == method_holders_specs.end()) {
                method_holders_specs.push_back({.id          = method_idt::identifier,    //
                                                .method_idts = {mem}});
            } else {
                it->method_idts.push_back(mem);
            }
        }
        struct method_holder_prep {
            const char* id;
            info        invoker_info;
        };
        return std::define_static_array(method_holders_specs | stdv::transform([](auto spec) {
                                            return method_holder_prep{
                                                spec.id, substitute(^^cvtmock_cvm_invoker, spec.method_idts)};
                                        }));
    }();

    auto trait_ref_args = vector<info>{};
    template for (constexpr auto spec: method_holders_specs) {
        trait_ref_args.push_back(get_cvtmock_holder<spec>());
    }
    return substitute(^^mock_ref_impl, trait_ref_args);
};
template<non_cv_trait Trait>
inline constexpr auto mock_ref_info = define_cvtmock_ref<Trait>();
}    // namespace cvtmock_trait

// cv-transient trait reference mock
template<non_cv_trait Trait>
using mock_trait_ref = [:cvtmock_trait::mock_ref_info<Trait>:];
}
