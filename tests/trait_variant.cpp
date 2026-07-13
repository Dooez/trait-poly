#include "trp/trait_variant.hpp"

#include "support/test_support.hpp"

#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

inline constexpr auto alternative_count        = 2;
inline constexpr auto first_index              = 0;
inline constexpr auto second_index             = 1;
inline constexpr auto throwing_value           = 0;
inline constexpr auto direct_value             = 5;
inline constexpr auto by_index_value           = 13;
inline constexpr auto first_value              = 7;
inline constexpr auto emplace_index_value      = 9;
inline constexpr auto emplace_type_value       = 10;
inline constexpr auto second_assign_value      = 11;
inline constexpr auto same_assign_value        = 12;
inline constexpr auto valueless_initial_value  = 3;
inline constexpr auto throwing_ctor_arg        = 0;
inline constexpr auto success_exit_code        = 0;
inline constexpr auto case_name                = std::string_view("trait_variant");

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
        return throwing_value;
    }
};

using variant_t           = trp::trait_variant<value_trait, first_impl, second_impl>;
using valueless_variant_t = trp::trait_variant<value_trait, first_impl, throwing_impl>;

static_assert(std::variant_size_v<variant_t> == alternative_count);
static_assert(std::variant_size_v<variant_t const> == alternative_count);
static_assert(std::same_as<std::variant_alternative_t<first_index, variant_t>, first_impl>);
static_assert(std::same_as<std::variant_alternative_t<second_index, variant_t const>, second_impl const>);

static_assert(std::same_as<decltype(get<first_index>(std::declval<variant_t&>())), first_impl&>);
static_assert(std::same_as<decltype(get<first_index>(std::declval<variant_t const&>())), first_impl const&>);
static_assert(std::same_as<decltype(get<first_impl>(std::declval<variant_t&&>())), first_impl&&>);

static_assert(std::same_as<decltype(get_if<first_index>(std::declval<variant_t*>())), first_impl*>);
static_assert(std::same_as<decltype(get_if<first_index>(std::declval<variant_t const*>())), first_impl const*>);
static_assert(std::same_as<decltype(emplace<first_index>(std::declval<variant_t&>(), first_value)), first_impl&>);
static_assert(std::same_as<decltype(emplace<second_impl>(std::declval<variant_t&>(), by_index_value)), second_impl&>);

int main() {
    auto direct = variant_t(first_impl{direct_value});

    test::expect_eq(case_name, "direct value ctor index", static_cast<int>(index(direct)), first_index);
    test::expect_eq(case_name, "direct value ctor", direct.value(), direct_value);

    auto by_index = variant_t(std::in_place_index<second_index>, by_index_value);
    test::expect_eq(case_name, "in_place_index ctor index", static_cast<int>(index(by_index)), second_index);
    test::expect_eq(case_name, "in_place_index ctor", by_index.value(), by_index_value);

    auto copied = variant_t(by_index);
    test::expect_eq(case_name, "copy ctor index", static_cast<int>(index(copied)), second_index);
    test::expect_eq(case_name, "copy ctor value", copied.value(), by_index_value);

    auto moved = variant_t(std::move(copied));
    test::expect_eq(case_name, "move ctor index", static_cast<int>(index(moved)), second_index);
    test::expect_eq(case_name, "move ctor value", moved.value(), by_index_value);

    auto var = variant_t(std::in_place_type<first_impl>, first_value);

    test::expect_eq(case_name, "index first", static_cast<int>(index(var)), first_index);
    test::expect_eq(case_name, "not valueless", valueless_by_exception(var), false);
    test::expect_eq(case_name, "holds_alternative first", holds_alternative<first_impl>(var), true);
    test::expect_eq(case_name, "holds_alternative second", holds_alternative<second_impl>(var), false);
    test::expect_eq(case_name, "get index", get<first_index>(var).value_, first_value);
    test::expect_eq(case_name, "get type", get<first_impl>(var).value(), first_value);
    test::expect_eq(case_name, "get_if index", get_if<first_index>(&var)->value_, first_value);
    test::expect_eq(case_name, "get_if type", get_if<first_impl>(&var)->value_, first_value);
    test::expect_eq(case_name, "get_if wrong index", get_if<second_index>(&var) == nullptr, true);
    test::expect_eq(case_name, "get_if wrong type", get_if<second_impl>(&var) == nullptr, true);
    auto* const null_var = static_cast<variant_t*>(nullptr);
    test::expect_eq(case_name, "get_if null index", get_if<first_index>(null_var) == nullptr, true);
    test::expect_eq(case_name, "get_if null type", get_if<first_impl>(null_var) == nullptr, true);

    try {
        (void)get<second_impl>(var);
        test::fail(case_name, "get should throw for inactive alternative");
    } catch (std::bad_variant_access const&) {}

    auto& replaced_by_index = emplace<second_index>(var, emplace_index_value);
    test::expect_eq(
        case_name, "emplace index returns active ref", replaced_by_index.value_, emplace_index_value);
    test::expect_eq(case_name, "emplace index updates index", static_cast<int>(index(var)), second_index);
    test::expect_eq(case_name, "emplace index updates value", var.value(), emplace_index_value);

    auto& replaced_by_type = emplace<first_impl>(var, emplace_type_value);
    test::expect_eq(
        case_name, "emplace type returns active ref", replaced_by_type.value_, emplace_type_value);
    test::expect_eq(case_name, "emplace type updates index", static_cast<int>(index(var)), first_index);
    test::expect_eq(case_name, "emplace type updates value", var.value(), emplace_type_value);

    var = first_impl{same_assign_value};
    test::expect_eq(case_name, "same alternative assignment index", static_cast<int>(index(var)), first_index);
    test::expect_eq(case_name, "same alternative assignment value", var.value(), same_assign_value);

    var = second_impl{second_assign_value};

    test::expect_eq(case_name, "reassigned index", static_cast<int>(index(var)), second_index);
    test::expect_eq(case_name, "holds_alternative reassigned", holds_alternative<second_impl>(var), true);
    test::expect_eq(
        case_name, "get const", get<second_index>(std::as_const(var)).value_, second_assign_value);
    test::expect_eq(case_name, "get rvalue", get<second_impl>(std::move(var)).value(), second_assign_value);

    auto valueless = valueless_variant_t(std::in_place_type<first_impl>, valueless_initial_value);
    try {
        (void)emplace<throwing_impl>(valueless, throwing_ctor_arg);
        test::fail(case_name, "emplace should throw");
    } catch (std::runtime_error const&) {}

    test::expect_eq(
        case_name, "valueless after failed emplace", valueless_by_exception(valueless), true);
    test::expect_eq(case_name, "get_if valueless", get_if<first_impl>(&valueless) == nullptr, true);
    try {
        (void)get<first_index>(valueless);
        test::fail(case_name, "get should throw for valueless variant");
    } catch (std::bad_variant_access const&) {}
    try {
        (void)valueless.value();
        test::fail(case_name, "valueless method should throw bad_variant_access");
    } catch (std::bad_variant_access const&) {}

    return success_exit_code;
}
