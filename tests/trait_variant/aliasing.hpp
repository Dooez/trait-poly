#pragma once

#include "support/test_support.hpp"
#include "trait_variant/support.hpp"
#include "trp/trait_variant.hpp"

#include <memory>

namespace aliasing {

inline constexpr auto case_name = std::string_view("variant_alias");

using empty_trait = marker<0>;

struct nonassignable {
    inline static bool copied_from_self = false;

    int value;

    explicit nonassignable(int initial)
    : value(initial) {}

    nonassignable(nonassignable const& other) {
        copied_from_self = this == std::addressof(other);
        value            = copied_from_self ? -1 : other.value;
    }

    nonassignable(nonassignable&&)                 = delete;
    nonassignable& operator=(nonassignable const&) = delete;
    nonassignable& operator=(nonassignable&&)      = delete;
};

inline void run() {
    auto value = trp::trait_variant<empty_trait, nonassignable>(std::in_place_type<nonassignable>, 42);

    auto& alias = trp::get<nonassignable>(value);
    value       = alias;

    test::expect_eq(case_name, "source remains alive", nonassignable::copied_from_self, false);
    test::expect_eq(case_name, "value preserved", trp::get<nonassignable>(value).value, 42);
}

}    // namespace aliasing
