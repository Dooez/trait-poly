#pragma once

#include "support/test_support.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace method_promotion {

inline constexpr auto case_name     = std::string_view("method_promotion");
inline constexpr auto lvalue_result = 1;
inline constexpr auto rvalue_result = 2;

struct base {
    auto value() -> int;
};

struct l_promoted : base {
    auto value() & noexcept -> int;
};

struct r_promoted : base {
    auto value() && noexcept -> int;
};

template<bool LNoexcept, bool RNoexcept>
struct impl {
    auto value() & noexcept(LNoexcept) -> int {
        return lvalue_result;
    }

    auto value() && noexcept(RNoexcept) -> int {
        return rvalue_result;
    }
};

using plain   = impl<false, false>;
using l_noex  = impl<true, false>;
using r_noex  = impl<false, true>;
using lr_noex = impl<true, true>;

// A split lvalue/rvalue pair satisfies the base's unqualified method.
static_assert(trp::implements_trait<plain, base>);

// clang-format off
// The similar lvalue declaration promotes only the lvalue requirement to
// noexcept. A throwing lvalue overload fails even if the rvalue overload is
// noexcept.
static_assert(!trp::implements_trait<plain,   l_promoted>);
static_assert( trp::implements_trait<l_noex,  l_promoted>);
static_assert(!trp::implements_trait<r_noex,  l_promoted>);
static_assert( trp::implements_trait<lr_noex, l_promoted>);

// The similar rvalue declaration promotes only the rvalue requirement to
// noexcept. A throwing rvalue overload fails even if the lvalue overload is
// noexcept.
static_assert(!trp::implements_trait<plain,   r_promoted>);
static_assert(!trp::implements_trait<l_noex,  r_promoted>);
static_assert( trp::implements_trait<r_noex,  r_promoted>);
static_assert( trp::implements_trait<lr_noex, r_promoted>);
// clang-format on

template<typename Trait, typename Impl>
void check_dispatch(std::string_view lvalue_check, std::string_view rvalue_check) {
    auto value = trp::trait_variant<Trait, Impl>(std::in_place_type<Impl>);

    test::expect_eq(case_name, lvalue_check, value.value(), lvalue_result);
    test::expect_eq(case_name, rvalue_check, std::move(value).value(), rvalue_result);
}

inline void run() {
    check_dispatch<l_promoted, l_noex>("promoted lvalue dispatch", "base rvalue dispatch");
    check_dispatch<r_promoted, r_noex>("base lvalue dispatch", "promoted rvalue dispatch");
}

}    // namespace method_promotion
