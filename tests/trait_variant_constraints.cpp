#include "support/test_support.hpp"
#include "trp/trait_variant.hpp"

#include <type_traits>
#include <utility>

namespace {

inline constexpr auto case_name = std::string_view("trait_variant_constraints");

struct value_trait {
    auto value() const -> int;
};

struct copy_error {};

struct throwing_copy {
    throwing_copy() = default;
    throwing_copy(throwing_copy const&) {
        throw copy_error{};
    }
    throwing_copy(throwing_copy&&) noexcept = default;

    auto value() const -> int {
        return 1;
    }
};

struct copy_only {
    int stored = 2;

    copy_only()                          = default;
    copy_only(copy_only const&) noexcept = default;
    copy_only(copy_only&&)               = delete;

    auto value() const -> int {
        return stored;
    }
};

struct move_only {
    move_only()                 = default;
    move_only(move_only const&) = delete;
    move_only(move_only&&)      = default;

    move_only& operator=(move_only const&) = delete;
    move_only& operator=(move_only&&)      = default;

    auto value() const -> int {
        return 3;
    }
};

using throwing_variant = trp::trait_variant<value_trait, throwing_copy>;
using copy_variant     = trp::trait_variant<value_trait, copy_only>;
using move_variant     = trp::trait_variant<value_trait, move_only>;

static_assert(!noexcept(throwing_variant(std::declval<throwing_copy&>())));
static_assert(std::is_constructible_v<copy_variant, copy_only&>);
static_assert(noexcept(copy_variant(std::declval<copy_only&>())));
static_assert(!std::is_constructible_v<move_variant, move_only&>);
static_assert(!std::is_assignable_v<move_variant&, move_only&>);

void check_lvalue_copy() {
    auto source = copy_only{};
    auto value  = copy_variant(source);

    test::expect_eq(case_name, "copy-only lvalue construction", value.value(), source.value());
}

void check_throwing_copy_propagates() {
    auto source = throwing_copy{};
    try {
        auto value = throwing_variant(source);
        (void)value;
        test::fail(case_name, "throwing copy should propagate");
    } catch (copy_error const&) {}
}

}    // namespace

int main() {
    check_lvalue_copy();
    check_throwing_copy_propagates();
}
