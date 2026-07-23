#pragma once

#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace static_methods {

inline constexpr auto case_name     = std::string_view("static_impl");
inline constexpr auto static_result = 41;
inline constexpr auto int_result    = 43;

struct split_trait {
    auto value() & -> int;
    auto value() && -> int;
};

struct static_impl {
    static auto value() -> int {
        return static_result;
    }
};

struct static_overload_trait {
    auto pick(short) const -> int;
};

struct static_overload_impl {
    static auto pick(long) -> int {
        return static_result;
    }

    static auto pick(int) -> int {
        return int_result;
    }
};

struct[[= trp::exact_cvref_signature]] exact_cvref_trait {
    auto value() & -> int;
};

struct[[= trp::matching_cv_signature]] exact_cv_trait {
    auto value() const -> int;
};

static_assert(trp::implements_trait<static_impl, split_trait>);
static_assert(trp::implements_trait<static_overload_impl, static_overload_trait const>);
static_assert(!trp::implements_trait<static_impl, exact_cvref_trait>);
static_assert(!trp::implements_trait<static_impl, exact_cv_trait>);

inline void run() {
    auto impl = static_impl{};
    auto ref  = trp::dyn_trait_ref<split_trait>(impl);
    auto var  = trp::trait_variant<split_trait, static_impl>(std::in_place_type<static_impl>);

    test::expect_eq(case_name, "dyn ref static dispatch", ref.value(), static_result);
    test::expect_eq(case_name, "variant static lvalue dispatch", var.value(), static_result);
    test::expect_eq(case_name, "variant static rvalue dispatch", std::move(var).value(), static_result);

    auto overload_impl = static_overload_impl{};
    auto overload_ref  = trp::dyn_trait_ref<static_overload_trait const>(overload_impl);
    test::expect_eq(case_name, "static overload resolution", overload_ref.pick(0), int_result);
}

}    // namespace static_methods
