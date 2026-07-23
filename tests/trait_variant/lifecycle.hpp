#pragma once

#include "support/test_support.hpp"
#include "trait_variant/support.hpp"
#include "trp/trait_variant.hpp"

#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

namespace lifecycle {

inline constexpr auto alternative_count     = 2;
inline constexpr auto direct_value          = 1;
inline constexpr auto duplicate_value       = 3;
inline constexpr auto impl_initial_value    = 4;
inline constexpr auto same_impl_value       = 5;
inline constexpr auto other_impl_value      = 6;
inline constexpr auto target_value          = 7;
inline constexpr auto source_value          = 8;
inline constexpr auto move_other_value      = 9;
inline constexpr auto move_same_value       = 10;
inline constexpr auto emplace_initial_value = 11;
inline constexpr auto emplace_type_value    = 12;
inline constexpr auto case_name             = std::string_view("trait_variant_lifecycle");

struct no_assign_impl {
    int stored = 0;

    explicit no_assign_impl(int value)
    : stored(value) {}

    no_assign_impl(no_assign_impl const&) = default;
    no_assign_impl(no_assign_impl&& other) noexcept
    : stored(other.stored) {
        other.stored = -1;
    }

    no_assign_impl& operator=(no_assign_impl const&) = delete;
    no_assign_impl& operator=(no_assign_impl&&)      = delete;

    auto value() const -> int {
        return stored;
    }
};

using other_impl        = value_impl<1>;
using lifecycle_variant = trp::trait_variant<value_trait, no_assign_impl, other_impl>;
using duplicate_variant = trp::trait_variant<value_trait, other_impl, other_impl>;

template<typename Var>
concept can_get_other_by_type = requires(Var& var) { get<other_impl>(var); };

static_assert(std::constructible_from<lifecycle_variant, no_assign_impl>);
static_assert(!std::convertible_to<no_assign_impl, lifecycle_variant>);
static_assert(!std::is_copy_assignable_v<no_assign_impl>);
static_assert(!std::is_move_assignable_v<no_assign_impl>);
static_assert(std::variant_size_v<duplicate_variant> == alternative_count);
static_assert(!std::constructible_from<duplicate_variant, other_impl>);
static_assert(!can_get_other_by_type<duplicate_variant>);

void check_construction() {
    auto direct = lifecycle_variant(no_assign_impl{direct_value});
    test::expect_eq(case_name, "direct construction index", static_cast<int>(index(direct)), 0);
    test::expect_eq(case_name, "direct construction value", direct.value(), direct_value);

    auto by_index = duplicate_variant(std::in_place_index<1>, duplicate_value);
    test::expect_eq(case_name, "duplicate index construction", static_cast<int>(index(by_index)), 1);
    test::expect_eq(case_name, "duplicate index get", get<1>(by_index).value(), duplicate_value);
}

void check_impl_assignment() {
    auto var = lifecycle_variant(std::in_place_type<no_assign_impl>, impl_initial_value);

    var = no_assign_impl{same_impl_value};
    test::expect_eq(case_name, "same impl assignment index", static_cast<int>(index(var)), 0);
    test::expect_eq(case_name, "same impl assignment value", var.value(), same_impl_value);

    var = other_impl{other_impl_value};
    test::expect_eq(case_name, "other impl assignment index", static_cast<int>(index(var)), 1);
    test::expect_eq(case_name, "other impl assignment value", var.value(), other_impl_value);
}

void check_variant_assignment() {
    auto target = lifecycle_variant(std::in_place_type<no_assign_impl>, target_value);
    auto source = lifecycle_variant(std::in_place_type<no_assign_impl>, source_value);

    target = source;
    test::expect_eq(case_name, "copy assignment same index", static_cast<int>(index(target)), 0);
    test::expect_eq(case_name, "copy assignment same value", target.value(), source_value);

    target = lifecycle_variant(std::in_place_type<other_impl>, move_other_value);
    test::expect_eq(case_name, "move assignment other index", static_cast<int>(index(target)), 1);
    test::expect_eq(case_name, "move assignment other value", target.value(), move_other_value);

    auto moved = lifecycle_variant(std::in_place_type<no_assign_impl>, move_same_value);
    target     = std::move(moved);
    test::expect_eq(case_name, "move assignment same index", static_cast<int>(index(target)), 0);
    test::expect_eq(case_name, "move assignment same value", target.value(), move_same_value);
}

void check_emplace() {
    auto var = lifecycle_variant(std::in_place_type<other_impl>, emplace_initial_value);

    auto& first = emplace<no_assign_impl>(var, emplace_type_value);
    test::expect_eq(case_name, "emplace type ref", first.value(), emplace_type_value);
    test::expect_eq(case_name, "emplace type index", static_cast<int>(index(var)), 0);
}

inline void run() {
    check_construction();
    check_impl_assignment();
    check_variant_assignment();
    check_emplace();
}

}    // namespace lifecycle
