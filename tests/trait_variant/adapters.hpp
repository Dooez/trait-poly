#pragma once

#include "support/ref_adapter_support.hpp"
#include "support/test_support.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace adapters {

inline constexpr auto case_name = std::string_view("trait_variant.reference_adapters");

template<typename Trait>
void check_lvalue(std::string_view check) {
    auto value = trp::trait_variant<Trait, impl>(std::in_place_type<impl>);
    test::expect_eq(case_name, check, value.access(), lvalue_result);
}

template<typename Trait>
void check_rvalue(std::string_view check) {
    auto value = trp::trait_variant<Trait, impl>(std::in_place_type<impl>);
    test::expect_eq(case_name, check, std::move(value).access(), rvalue_result);
}

inline void run() {
    check_lvalue<lvalue_default_trait>("inline lvalue default");
    check_lvalue<reftest::lvalue_trait>("explicit lvalue implementation");
    check_rvalue<rvalue_default_trait>("inline rvalue default");
    check_rvalue<reftest::rvalue_trait>("explicit rvalue implementation");
}

}    // namespace adapters
