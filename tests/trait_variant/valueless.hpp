#pragma once

#include "support/test_support.hpp"
#include "trait_variant/support.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace valueless {

inline constexpr auto case_name      = std::string_view("trait_variant.valueless");
inline constexpr auto initial_value  = 1;
inline constexpr auto copy_value     = 2;
inline constexpr auto move_value     = 3;
inline constexpr auto emplace_value  = 4;
inline constexpr auto throwing_value = 5;

using construction_error = valuetest::construction_error;
using copy_error         = valuetest::copy_error;
using move_error         = valuetest::move_error;
using stable_impl        = stable_value_impl;
using throwing_impl      = throwing_value_impl;
using variant_t          = ::valueless_variant_t;

void make_valueless(variant_t& value) {
    throwing_impl::fail_construction = true;
    try {
        (void)trp::emplace<throwing_impl>(value, throwing_value);
        test::fail(case_name, "throwing emplace should fail");
    } catch (construction_error const&) {
        throwing_impl::fail_construction = false;
    }
    test::expect_eq(
        case_name, "failed emplace creates valueless state", trp::valueless_by_exception(value), true);
}

void check_construction() {
    throwing_impl::reset();
    auto source = variant_t(std::in_place_type<stable_impl>, initial_value);
    make_valueless(source);

    auto copied = variant_t(source);
    test::expect_eq(
        case_name, "copy construction preserves valueless state", trp::valueless_by_exception(copied), true);

    auto moved = variant_t(std::move(source));
    test::expect_eq(
        case_name, "move construction preserves valueless state", trp::valueless_by_exception(moved), true);
    test::expect_eq(
        case_name, "move construction keeps source valueless", trp::valueless_by_exception(source), true);

    copied = copied;
    test::expect_eq(
        case_name, "valueless self copy remains valueless", trp::valueless_by_exception(copied), true);

    moved = std::move(moved);
    test::expect_eq(
        case_name, "valueless self move remains valueless", trp::valueless_by_exception(moved), true);
}

void check_assignment_from_valueless() {
    throwing_impl::reset();
    auto source = variant_t(std::in_place_type<stable_impl>, initial_value);
    make_valueless(source);

    auto copy_target = variant_t(std::in_place_type<stable_impl>, copy_value);
    copy_target      = source;
    test::expect_eq(
        case_name, "copy assignment from valueless", trp::valueless_by_exception(copy_target), true);
    test::expect_eq(case_name, "copy source remains valueless", trp::valueless_by_exception(source), true);

    auto move_target = variant_t(std::in_place_type<stable_impl>, move_value);
    move_target      = std::move(source);
    test::expect_eq(
        case_name, "move assignment from valueless", trp::valueless_by_exception(move_target), true);
    test::expect_eq(case_name, "move source remains valueless", trp::valueless_by_exception(source), true);
}

void check_recovery() {
    throwing_impl::reset();

    auto emplaced = variant_t(std::in_place_type<stable_impl>, initial_value);
    make_valueless(emplaced);
    auto& recovered = trp::emplace<stable_impl>(emplaced, emplace_value);
    test::expect_eq(
        case_name, "emplace recovers valueless state", trp::valueless_by_exception(emplaced), false);
    test::expect_eq(case_name, "emplace recovery value", recovered.value(), emplace_value);

    auto copy_target = variant_t(std::in_place_type<stable_impl>, initial_value);
    make_valueless(copy_target);
    auto copy_source = variant_t(std::in_place_type<stable_impl>, copy_value);
    copy_target      = copy_source;
    test::expect_eq(case_name,
                    "copy assignment recovers valueless state",
                    trp::valueless_by_exception(copy_target),
                    false);
    test::expect_eq(case_name, "copy recovery value", copy_target.value(), copy_value);

    auto move_target = variant_t(std::in_place_type<stable_impl>, initial_value);
    make_valueless(move_target);
    auto move_source = variant_t(std::in_place_type<stable_impl>, move_value);
    move_target      = std::move(move_source);
    test::expect_eq(case_name,
                    "move assignment recovers valueless state",
                    trp::valueless_by_exception(move_target),
                    false);
    test::expect_eq(case_name, "move recovery value", move_target.value(), move_value);
}

void check_throwing_replacement() {
    throwing_impl::reset();

    auto copy_target         = variant_t(std::in_place_type<stable_impl>, initial_value);
    auto copy_source         = variant_t(std::in_place_type<throwing_impl>, throwing_value);
    throwing_impl::fail_copy = true;
    try {
        copy_target = copy_source;
        test::fail(case_name, "throwing copy replacement should fail");
    } catch (copy_error const&) {
        throwing_impl::fail_copy = false;
    }
    test::expect_eq(
        case_name, "throwing copy replacement is valueless", trp::valueless_by_exception(copy_target), true);
    test::expect_eq(case_name, "throwing copy source remains valued", copy_source.value(), throwing_value);

    auto move_target         = variant_t(std::in_place_type<stable_impl>, initial_value);
    auto move_source         = variant_t(std::in_place_type<throwing_impl>, throwing_value);
    throwing_impl::fail_move = true;
    try {
        move_target = std::move(move_source);
        test::fail(case_name, "throwing move replacement should fail");
    } catch (move_error const&) {
        throwing_impl::fail_move = false;
    }
    test::expect_eq(
        case_name, "throwing move replacement is valueless", trp::valueless_by_exception(move_target), true);
    test::expect_eq(case_name, "throwing move source remains valued", move_source.value(), throwing_value);
}

inline void run() {
    check_construction();
    check_assignment_from_valueless();
    check_recovery();
    check_throwing_replacement();
}

}    // namespace valueless
