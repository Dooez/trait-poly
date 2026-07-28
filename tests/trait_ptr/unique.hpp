#pragma once

#include "support/test_support.hpp"
#include "trait_ptr/support.hpp"
#include "trp/unique_trait_ptr.hpp"

#include <cstddef>
#include <memory>
#include <utility>

namespace unique {

inline constexpr auto initial_value   = 1;
inline constexpr auto updated_value   = 2;
inline constexpr auto moved_value     = 3;
inline constexpr auto replaced_value  = 4;
inline constexpr auto cast_value      = 5;
inline constexpr auto downcast_value  = 6;
inline constexpr auto alloc_value     = 7;
inline constexpr auto converted_value = 8;
inline constexpr auto failed_value    = 9;
inline constexpr auto released_value  = 10;
inline constexpr auto assigned_value  = 11;
inline constexpr auto replaced_alloc  = 12;
inline constexpr auto alloc_cast      = 13;
inline constexpr auto alias_value     = 14;
inline constexpr auto case_name       = std::string_view("unique_trait_ptr_ownership");

struct unique_assignment_owner {
    trp::unique_trait_ptr<read_trait> replacement;

    explicit unique_assignment_owner(trp::unique_trait_ptr<read_trait> value)
    : replacement(std::move(value)) {}

    auto value() const -> int {
        return -1;
    }
};

struct alloc_assignment_owner {
    trp::alloc_unique_trait_ptr<read_trait> replacement;

    explicit alloc_assignment_owner(trp::alloc_unique_trait_ptr<read_trait> value)
    : replacement(std::move(value)) {}

    auto value() const -> int {
        return -1;
    }
};

void check_unique_destroys_once() {
    auto state = counts{};
    {
        auto ptr = trp::make_unique_trait<write_trait, node>(state, initial_value);
        test::expect_eq(case_name, "live after make", state.alive, 1);
        test::expect_eq(case_name, "bool", static_cast<bool>(ptr), true);
        test::expect_eq(case_name, "typed get", ptr.get<node>() != nullptr, true);

        ptr->set(updated_value);
        test::expect_eq(case_name, "method call", ptr->value(), updated_value);
    }
    test::expect_eq(case_name, "live after scope", state.alive, 0);
    test::expect_eq(case_name, "destroyed after scope", state.destroyed, 1);
}

void check_unique_moves() {
    auto state = counts{};
    {
        auto source = trp::make_unique_trait<write_trait, node>(state, moved_value);
        auto moved  = std::move(source);

        test::expect_eq(case_name, "moved-from empty", static_cast<bool>(source), false);
        test::expect_eq(case_name, "moved-to value", moved->value(), moved_value);

        auto target = trp::make_unique_trait<write_trait, node>(state, replaced_value);
        target      = std::move(moved);

        test::expect_eq(case_name, "assignment destroys old", state.destroyed, 1);
        test::expect_eq(case_name, "move assignment source empty", static_cast<bool>(moved), false);
        test::expect_eq(case_name, "move assignment value", target->value(), moved_value);
    }
    test::expect_eq(case_name, "move live after scope", state.alive, 0);
    test::expect_eq(case_name, "move destroyed after scope", state.destroyed, 2);
}

void check_unique_casts() {
    auto state = counts{};
    {
        auto ptr  = trp::make_unique_trait<write_trait, node>(state, cast_value);
        auto base = trp::trait_cast<read_trait>(std::move(ptr));

        test::expect_eq(case_name, "upcast empties source", static_cast<bool>(ptr), false);
        test::expect_eq(case_name, "upcast value", base->value(), cast_value);

        auto down = trp::trait_cast<write_trait, node>(std::move(base));
        test::expect_eq(case_name, "downcast empties source", static_cast<bool>(base), false);
        test::expect_eq(case_name, "downcast value", down->value(), cast_value);
        down->set(downcast_value);
        test::expect_eq(case_name, "downcast write", down->value(), downcast_value);
    }
    test::expect_eq(case_name, "cast live after scope", state.alive, 0);
    test::expect_eq(case_name, "cast destroyed after scope", state.destroyed, 1);
}

void check_alloc_unique_paths() {
    auto state = counts{};
    {
        auto ptr =
            trp::allocate_unique_trait<write_trait, node>(std::allocator<std::byte>{}, state, alloc_value);
        test::expect_eq(case_name, "alloc unique value", ptr->value(), alloc_value);
    }
    test::expect_eq(case_name, "alloc unique destroyed", state.destroyed, 1);

    {
        auto ptr = trp::make_unique_trait<write_trait, node>(state, converted_value);
        trp::alloc_unique_trait_ptr<write_trait> alloc_ptr = std::move(ptr);

        test::expect_eq(case_name, "converted unique empty", static_cast<bool>(ptr), false);
        test::expect_eq(case_name, "converted alloc unique value", alloc_ptr->value(), converted_value);
    }
    test::expect_eq(case_name, "converted alloc unique destroyed", state.destroyed, 2);
}

void check_failed_downcasts_preserve_source() {
    auto state = counts{};
    {
        auto source = trp::make_unique_trait<read_trait, read_node>(state, failed_value);
        auto failed = trp::trait_cast<write_trait, node>(std::move(source));

        test::expect_eq(case_name, "failed unique downcast empty", static_cast<bool>(failed), false);
        test::expect_eq(case_name, "failed unique downcast keeps source", static_cast<bool>(source), true);
        test::expect_eq(case_name, "failed unique downcast source value", source->value(), failed_value);
    }
    test::expect_eq(case_name, "failed unique downcast destroys source", state.destroyed, 1);

    state = {};
    {
        auto source = trp::allocate_unique_trait<read_trait, read_node>(
            std::allocator<std::byte>{}, state, failed_value);
        auto failed = trp::trait_cast<write_trait, node>(std::move(source));

        test::expect_eq(case_name, "failed allocated downcast empty", static_cast<bool>(failed), false);
        test::expect_eq(case_name, "failed allocated downcast keeps source", static_cast<bool>(source), true);
        test::expect_eq(case_name, "failed allocated downcast source value", source->value(), failed_value);
    }
    test::expect_eq(case_name, "failed allocated downcast destroys source", state.destroyed, 1);
}

void check_release() {
    auto  state = counts{};
    auto  ptr   = trp::make_unique_trait<write_trait, node>(state, released_value);
    auto* raw   = static_cast<node*>(ptr.release());

    test::expect_eq(case_name, "release empties owner", static_cast<bool>(ptr), false);
    test::expect_eq(case_name, "release returns object", raw != nullptr, true);
    test::expect_eq(case_name, "released value", raw->value(), released_value);
    test::expect_eq(case_name, "released object remains alive", state.alive, 1);
    test::expect_eq(case_name, "release does not destroy", state.destroyed, 0);

    delete raw;
    test::expect_eq(case_name, "caller ends released lifetime", state.alive, 0);
    test::expect_eq(case_name, "caller destroys released object", state.destroyed, 1);

    auto empty = trp::unique_trait_ptr<read_trait>{};
    test::expect_eq(case_name, "release empty returns null", empty.release() == nullptr, true);
}

void check_alloc_unique_move_assignment() {
    auto state = counts{};
    {
        auto source =
            trp::allocate_unique_trait<write_trait, node>(std::allocator<std::byte>{}, state, assigned_value);
        auto target =
            trp::allocate_unique_trait<write_trait, node>(std::allocator<std::byte>{}, state, replaced_alloc);

        target = std::move(source);
        test::expect_eq(case_name, "allocated move destroys target", state.destroyed, 1);
        test::expect_eq(case_name, "allocated move empties source", static_cast<bool>(source), false);
        test::expect_eq(case_name, "allocated move transfers value", target->value(), assigned_value);

        target = std::move(target);
        test::expect_eq(case_name, "allocated self move preserves value", target->value(), assigned_value);
        test::expect_eq(case_name, "allocated self move destroys nothing", state.destroyed, 1);
    }
    test::expect_eq(case_name, "allocated move destroys both objects", state.destroyed, 2);
}

void check_assignment_aliases() {
    using replacement_type = valuetest::value_impl<0>;

    auto replacement = trp::make_unique_trait<read_trait, replacement_type>(alias_value);
    auto target      = trp::make_unique_trait<read_trait, unique_assignment_owner>(std::move(replacement));

    target = std::move(target.get<unique_assignment_owner>()->replacement);

    test::expect_eq(case_name, "aliased unique assignment keeps owner", static_cast<bool>(target), true);
    test::expect_eq(case_name, "aliased unique assignment value", target->value(), alias_value);

    auto allocator         = std::allocator<std::byte>{};
    auto alloc_replacement = trp::allocate_unique_trait<read_trait, replacement_type>(allocator, alias_value);
    auto alloc_target      = trp::allocate_unique_trait<read_trait, alloc_assignment_owner>(
        allocator, std::move(alloc_replacement));

    alloc_target = std::move(alloc_target.get<alloc_assignment_owner>()->replacement);

    test::expect_eq(
        case_name, "aliased allocated assignment keeps owner", static_cast<bool>(alloc_target), true);
    test::expect_eq(case_name, "aliased allocated assignment value", alloc_target->value(), alias_value);
}

void check_alloc_unique_casts() {
    auto state = counts{};
    {
        auto ptr =
            trp::allocate_unique_trait<write_trait, node>(std::allocator<std::byte>{}, state, alloc_cast);
        auto base = trp::trait_cast<read_trait>(std::move(ptr));

        test::expect_eq(case_name, "allocated upcast empties source", static_cast<bool>(ptr), false);
        test::expect_eq(case_name, "allocated upcast value", base->value(), alloc_cast);

        auto down = trp::trait_cast<write_trait, node>(std::move(base));
        test::expect_eq(case_name, "allocated downcast empties source", static_cast<bool>(base), false);
        test::expect_eq(case_name, "allocated downcast value", down->value(), alloc_cast);
    }
    test::expect_eq(case_name, "allocated cast destroys object", state.destroyed, 1);
}

inline void run() {
    check_unique_destroys_once();
    check_unique_moves();
    check_unique_casts();
    check_alloc_unique_paths();
    check_failed_downcasts_preserve_source();
    check_release();
    check_alloc_unique_move_assignment();
    check_assignment_aliases();
    check_alloc_unique_casts();
}

}    // namespace unique
