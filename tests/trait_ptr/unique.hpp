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
inline constexpr auto case_name       = std::string_view("unique_trait_ptr_ownership");

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

inline void run() {
    check_unique_destroys_once();
    check_unique_moves();
    check_unique_casts();
    check_alloc_unique_paths();
}

}    // namespace unique
