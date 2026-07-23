#pragma once

#include "support/forwarding_support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

#include <concepts>
#include <string_view>
#include <utility>

namespace forwarding {

inline constexpr auto case_name = std::string_view("dyn_trait_ref.forwarding");

using ref_t          = trp::dyn_trait_ref<forwardtest::reference_trait>;
using prvalue_ref_t  = trp::dyn_trait_ref<forwardtest::prvalue_trait>;
using argument_ref_t = trp::dyn_trait_ref<forwardtest::argument_trait>;

static_assert(trp::implements_trait<forwardtest::reference_impl, forwardtest::reference_trait>);
static_assert(std::same_as<decltype(std::declval<ref_t&>().mutable_value()), int&>);
static_assert(std::same_as<decltype(std::declval<ref_t const&>().const_value()), int const&>);

static_assert(trp::implements_trait<forwardtest::prvalue_impl, forwardtest::prvalue_trait>);
static_assert(
    std::same_as<decltype(std::declval<prvalue_ref_t&>().make_move_only()), forwardtest::move_only_type>);
static_assert(
    std::same_as<decltype(std::declval<prvalue_ref_t&>().make_immovable()), forwardtest::immovable_type>);

static_assert(trp::implements_trait<forwardtest::argument_impl, forwardtest::argument_trait>);

inline void check_reference_returns() {
    auto object = forwardtest::reference_impl{};
    auto ref    = ref_t(object);

    auto& mutable_value = ref.mutable_value();
    test::expect_eq(case_name, "mutable reference identity", &mutable_value == &object.stored, true);

    mutable_value = forwardtest::changed_value;
    test::expect_eq(case_name, "mutable reference writes object", object.stored, forwardtest::changed_value);

    auto const& const_object = std::as_const(object);
    auto const& const_value  = std::as_const(ref).const_value();
    test::expect_eq(case_name, "const reference identity", &const_value == &const_object.stored, true);
    test::expect_eq(case_name, "const reference reads object", const_value, forwardtest::changed_value);
}

inline void check_prvalue_returns() {
    auto object = forwardtest::prvalue_impl{};
    auto ref    = prvalue_ref_t(object);

    auto move_only = ref.make_move_only();
    test::expect_eq(case_name, "move-only prvalue value", move_only.value, forwardtest::move_only_value);

    auto immovable = ref.make_immovable();
    test::expect_eq(case_name, "immovable prvalue value", immovable.value, forwardtest::immovable_value);
}

inline void check_arguments() {
    auto object = forwardtest::argument_impl{};
    auto ref    = argument_ref_t(object);

    auto lvalue = forwardtest::lvalue_argument;
    ref.mutate(lvalue);
    test::expect_eq(case_name, "lvalue argument identity", object.lvalue_address == &lvalue, true);
    test::expect_eq(case_name, "lvalue argument mutation", lvalue, forwardtest::lvalue_forwarded);

    auto by_value = forwardtest::move_only_type(forwardtest::by_value_argument);
    ref.take_value(std::move(by_value));
    test::expect_eq(case_name, "move-only value argument", object.by_value, forwardtest::by_value_argument);
    test::expect_eq(case_name, "move-only value source", by_value.value, forwardtest::moved_from_value);

    auto rvalue = forwardtest::move_only_type(forwardtest::rvalue_argument);
    ref.take_rvalue(std::move(rvalue));
    test::expect_eq(case_name, "rvalue argument value", object.rvalue, forwardtest::rvalue_argument);
    test::expect_eq(case_name, "rvalue argument identity", object.rvalue_address == &rvalue, true);
    test::expect_eq(case_name, "rvalue argument mutation", rvalue.value, forwardtest::rvalue_forwarded);
}

inline void run() {
    check_reference_returns();
    check_prvalue_returns();
    check_arguments();
}

}    // namespace forwarding
