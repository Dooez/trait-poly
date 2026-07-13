#include "support/test_support.hpp"
#include "support/trait_ptr_support.hpp"
#include "trp/shared_trait_ptr.hpp"

#include <utility>

inline constexpr auto copy_initial_value      = 9;
inline constexpr auto copy_updated_value      = 10;
inline constexpr auto first_assignment_value  = 11;
inline constexpr auto second_assignment_value = 12;
inline constexpr auto cast_initial_value      = 13;
inline constexpr auto cast_updated_value      = 14;
inline constexpr auto wrong_cast_value        = 15;
inline constexpr auto success_exit_code       = 0;
inline constexpr auto case_name               = std::string_view("shared_trait_ptr_ownership");

void check_shared_copies() {
    auto state = counts{};
    {
        auto first = trp::make_shared_trait<write_trait, node>(state, copy_initial_value);
        {
            auto second = first;
            test::expect_eq(case_name, "copy keeps one object", state.alive, 1);
            second->set(copy_updated_value);
            test::expect_eq(case_name, "copy writes same object", first->value(), copy_updated_value);

            auto third = std::move(second);
            test::expect_eq(case_name, "moved-from empty", static_cast<bool>(second), false);
            test::expect_eq(case_name, "moved value", third->value(), copy_updated_value);

            first = first;
            test::expect_eq(case_name, "self assignment keeps value", first->value(), copy_updated_value);
        }
        test::expect_eq(case_name, "object alive after copies", state.alive, 1);
        test::expect_eq(case_name, "not destroyed early", state.destroyed, 0);
    }
    test::expect_eq(case_name, "destroyed after last owner", state.destroyed, 1);
}

void check_shared_assignment() {
    auto state = counts{};
    {
        auto first  = trp::make_shared_trait<write_trait, node>(state, first_assignment_value);
        auto second = trp::make_shared_trait<write_trait, node>(state, second_assignment_value);

        first = second;
        test::expect_eq(case_name, "assignment destroys old", state.destroyed, 1);
        test::expect_eq(case_name, "assignment keeps new alive", state.alive, 1);
        test::expect_eq(case_name, "assignment value", first->value(), second_assignment_value);
    }
    test::expect_eq(case_name, "assignment destroyed after scope", state.destroyed, 2);
}

void check_shared_casts() {
    auto state = counts{};
    {
        auto derived = trp::make_shared_trait<write_trait, node>(state, cast_initial_value);
        {
            auto base = trp::trait_cast<read_trait>(derived);
            test::expect_eq(case_name, "upcast value", base->value(), cast_initial_value);
            test::expect_eq(case_name, "upcast keeps original", derived->value(), cast_initial_value);

            auto down = trp::trait_cast<write_trait, node>(base);
            down->set(cast_updated_value);
            test::expect_eq(case_name, "downcast writes same object", derived->value(), cast_updated_value);
        }
        test::expect_eq(case_name, "cast object still alive", state.alive, 1);
    }
    test::expect_eq(case_name, "cast destroyed after scope", state.destroyed, 1);
}

void check_shared_wrong_cast() {
    auto state = counts{};
    {
        auto ptr   = trp::make_shared_trait<read_trait, read_node>(state, wrong_cast_value);
        auto wrong = trp::trait_cast<write_trait, node>(ptr);

        test::expect_eq(case_name, "wrong cast empty", static_cast<bool>(wrong), false);
        test::expect_eq(case_name, "wrong cast keeps source", ptr->value(), wrong_cast_value);
        test::expect_eq(case_name, "wrong cast not destroyed early", state.destroyed, 0);
    }
    test::expect_eq(case_name, "wrong cast destroyed after scope", state.destroyed, 1);
}

int main() {
    check_shared_copies();
    check_shared_assignment();
    check_shared_casts();
    check_shared_wrong_cast();

    return success_exit_code;
}
