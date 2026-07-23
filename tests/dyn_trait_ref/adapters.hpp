#pragma once

#include "support/ref_adapter_support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace adapters {

inline constexpr auto case_name = std::string_view("dyn_trait_ref.reference_adapters");

inline void run() {
    auto object = impl{};

    auto default_ref = trp::dyn_trait_ref<lvalue_default_trait>(object);
    test::expect_eq(case_name, "inline lvalue default", default_ref.access(), lvalue_result);

    auto explicit_ref = trp::dyn_trait_ref<reftest::lvalue_trait>(object);
    test::expect_eq(case_name, "explicit lvalue implementation", explicit_ref.access(), lvalue_result);

    // Rvalue-only methods are not exposed by the non-owning view, but their
    // adapters must still be constructible.
    (void)trp::dyn_trait_ref<rvalue_default_trait>(object);
    (void)trp::dyn_trait_ref<reftest::rvalue_trait>(object);
}

}    // namespace adapters
