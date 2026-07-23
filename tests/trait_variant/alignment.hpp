#pragma once

#include "support/test_support.hpp"
#include "trait_variant/support.hpp"
#include "trp/trait_variant.hpp"

#include <cstddef>
#include <cstdint>

namespace alignment {

inline constexpr auto case_name      = std::string_view("trait_variant.alignment");
inline constexpr auto initial_value  = 1;
inline constexpr auto replaced_value = 2;

struct alignas(128) aligned_impl {
    int stored;

    explicit aligned_impl(int value)
    : stored(value) {}

    auto value() const -> int {
        return stored;
    }
};

using other_impl = value_impl<3>;
using variant_t  = trp::trait_variant<value_trait, other_impl, aligned_impl>;

static_assert(alignof(aligned_impl) > alignof(std::max_align_t));
static_assert(alignof(variant_t) >= alignof(aligned_impl));

void check_address(aligned_impl const& value, std::string_view check) {
    auto const address = reinterpret_cast<std::uintptr_t>(&value);
    test::expect_eq(case_name, check, address % alignof(aligned_impl), std::uintptr_t{0});
}

inline void run() {
    auto value = variant_t(std::in_place_type<aligned_impl>, initial_value);

    test::expect_eq(case_name, "initial aligned value", value.value(), initial_value);
    check_address(trp::get<aligned_impl>(value), "initial aligned address");

    value          = other_impl{initial_value};
    auto& replaced = trp::emplace<aligned_impl>(value, replaced_value);
    test::expect_eq(case_name, "replacement aligned value", value.value(), replaced_value);
    check_address(replaced, "replacement aligned address");
}

}    // namespace alignment
