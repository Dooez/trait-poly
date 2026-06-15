#include "trp/trait_variant.hpp"

#include "test_support.hpp"

#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

struct value_trait {
    auto value() const -> int;
};

struct first_impl {
    int value_;

    explicit first_impl(int value)
    : value_(value) {}

    auto value() const -> int {
        return value_;
    }
};

struct second_impl {
    int value_;

    explicit second_impl(int value)
    : value_(value) {}

    auto value() const -> int {
        return value_;
    }
};

struct throwing_impl {
    explicit throwing_impl(int) {
        throw std::runtime_error{"throwing_impl"};
    }

    auto value() const -> int {
        return 0;
    }
};

using variant_t           = trp::trait_variant<value_trait, first_impl, second_impl>;
using valueless_variant_t = trp::trait_variant<value_trait, first_impl, throwing_impl>;

static_assert(std::variant_size_v<variant_t> == 2);
static_assert(std::variant_size_v<variant_t const> == 2);
static_assert(std::same_as<std::variant_alternative_t<0, variant_t>, first_impl>);
static_assert(std::same_as<std::variant_alternative_t<1, variant_t const>, second_impl const>);

static_assert(std::same_as<decltype(get<0>(std::declval<variant_t&>())), first_impl&>);
static_assert(std::same_as<decltype(get<0>(std::declval<variant_t const&>())), first_impl const&>);
static_assert(std::same_as<decltype(get<first_impl>(std::declval<variant_t&&>())), first_impl&&>);

static_assert(std::same_as<decltype(get_if<0>(std::declval<variant_t*>())), first_impl*>);
static_assert(std::same_as<decltype(get_if<0>(std::declval<variant_t const*>())), first_impl const*>);
static_assert(std::same_as<decltype(emplace<0>(std::declval<variant_t&>(), 1)), first_impl&>);
static_assert(std::same_as<decltype(emplace<second_impl>(std::declval<variant_t&>(), 1)), second_impl&>);

int main() {
    auto direct = variant_t(first_impl{5});

    test::expect_eq("trait_variant", "direct value ctor index", static_cast<int>(index(direct)), 0);
    test::expect_eq("trait_variant", "direct value ctor", direct.value(), 5);

    auto by_index = variant_t(std::in_place_index<1>, 13);
    test::expect_eq("trait_variant", "in_place_index ctor index", static_cast<int>(index(by_index)), 1);
    test::expect_eq("trait_variant", "in_place_index ctor", by_index.value(), 13);

    auto copied = variant_t(by_index);
    test::expect_eq("trait_variant", "copy ctor index", static_cast<int>(index(copied)), 1);
    test::expect_eq("trait_variant", "copy ctor value", copied.value(), 13);

    auto moved = variant_t(std::move(copied));
    test::expect_eq("trait_variant", "move ctor index", static_cast<int>(index(moved)), 1);
    test::expect_eq("trait_variant", "move ctor value", moved.value(), 13);

    auto var = variant_t(std::in_place_type<first_impl>, 7);

    test::expect_eq("trait_variant", "index first", static_cast<int>(index(var)), 0);
    test::expect_eq("trait_variant", "not valueless", valueless_by_exception(var), false);
    test::expect_eq("trait_variant", "holds_alternative first", holds_alternative<first_impl>(var), true);
    test::expect_eq("trait_variant", "holds_alternative second", holds_alternative<second_impl>(var), false);
    test::expect_eq("trait_variant", "get index", get<0>(var).value_, 7);
    test::expect_eq("trait_variant", "get type", get<first_impl>(var).value(), 7);
    test::expect_eq("trait_variant", "get_if index", get_if<0>(&var)->value_, 7);
    test::expect_eq("trait_variant", "get_if type", get_if<first_impl>(&var)->value_, 7);
    test::expect_eq("trait_variant", "get_if wrong index", get_if<1>(&var) == nullptr, true);
    test::expect_eq("trait_variant", "get_if wrong type", get_if<second_impl>(&var) == nullptr, true);
    auto* const null_var = static_cast<variant_t*>(nullptr);
    test::expect_eq("trait_variant", "get_if null index", get_if<0>(null_var) == nullptr, true);
    test::expect_eq("trait_variant", "get_if null type", get_if<first_impl>(null_var) == nullptr, true);

    try {
        (void)get<second_impl>(var);
        test::fail("trait_variant", "get should throw for inactive alternative");
    } catch (std::bad_variant_access const&) {}

    auto& replaced_by_index = emplace<1>(var, 9);
    test::expect_eq("trait_variant", "emplace index returns active ref", replaced_by_index.value_, 9);
    test::expect_eq("trait_variant", "emplace index updates index", static_cast<int>(index(var)), 1);
    test::expect_eq("trait_variant", "emplace index updates value", var.value(), 9);

    auto& replaced_by_type = emplace<first_impl>(var, 10);
    test::expect_eq("trait_variant", "emplace type returns active ref", replaced_by_type.value_, 10);
    test::expect_eq("trait_variant", "emplace type updates index", static_cast<int>(index(var)), 0);
    test::expect_eq("trait_variant", "emplace type updates value", var.value(), 10);

    var = first_impl{12};
    test::expect_eq("trait_variant", "same alternative assignment index", static_cast<int>(index(var)), 0);
    test::expect_eq("trait_variant", "same alternative assignment value", var.value(), 12);

    var = second_impl{11};

    test::expect_eq("trait_variant", "reassigned index", static_cast<int>(index(var)), 1);
    test::expect_eq(
        "trait_variant", "holds_alternative reassigned", holds_alternative<second_impl>(var), true);
    test::expect_eq("trait_variant", "get const", get<1>(std::as_const(var)).value_, 11);
    test::expect_eq("trait_variant", "get rvalue", get<second_impl>(std::move(var)).value(), 11);

    auto valueless = valueless_variant_t(std::in_place_type<first_impl>, 3);
    try {
        (void)emplace<throwing_impl>(valueless, 0);
        test::fail("trait_variant", "emplace should throw");
    } catch (std::runtime_error const&) {}

    test::expect_eq(
        "trait_variant", "valueless after failed emplace", valueless_by_exception(valueless), true);
    test::expect_eq(
        "trait_variant", "get_if valueless", get_if<first_impl>(&valueless) == nullptr, true);
    try {
        (void)get<0>(valueless);
        test::fail("trait_variant", "get should throw for valueless variant");
    } catch (std::bad_variant_access const&) {}
    try {
        (void)valueless.value();
        test::fail("trait_variant", "valueless method should throw bad_variant_access");
    } catch (std::bad_variant_access const&) {}

    return 0;
}
