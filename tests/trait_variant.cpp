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

int main() {
    auto var = variant_t(std::in_place_type<first_impl>, 7);

    test::expect_eq("trait_variant", "holds_alternative first", holds_alternative<first_impl>(var), true);
    test::expect_eq("trait_variant", "holds_alternative second", holds_alternative<second_impl>(var), false);
    test::expect_eq("trait_variant", "get index", get<0>(var).value_, 7);
    test::expect_eq("trait_variant", "get type", get<first_impl>(var).value(), 7);
    test::expect_eq("trait_variant", "get_if index", get_if<0>(&var)->value_, 7);
    test::expect_eq("trait_variant", "get_if type", get_if<first_impl>(&var)->value_, 7);
    test::expect_eq("trait_variant", "get_if wrong index", get_if<1>(&var) == nullptr, true);
    test::expect_eq("trait_variant", "get_if wrong type", get_if<second_impl>(&var) == nullptr, true);

    try {
        (void)get<second_impl>(var);
        test::fail("trait_variant", "get should throw for inactive alternative");
    } catch (std::bad_variant_access const&) {}

    var = second_impl{11};

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
    try {
        (void)valueless.value();
        test::fail("trait_variant", "valueless method should throw bad_variant_access");
    } catch (std::bad_variant_access const&) {}

    return 0;
}
