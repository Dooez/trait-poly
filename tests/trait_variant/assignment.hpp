#pragma once

#include "support/test_support.hpp"
#include "trait_variant/support.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace assignment {

inline constexpr auto case_name        = std::string_view("trait_variant.assignment");
inline constexpr auto initial_value    = 1;
inline constexpr auto copy_value       = 2;
inline constexpr auto move_value       = 3;
inline constexpr auto other_value      = 4;
inline constexpr auto recovered_value  = 5;
inline constexpr auto moved_from_value = -1;

struct tracked_impl {
    inline static int live             = 0;
    inline static int destroyed        = 0;
    inline static int copy_assignments = 0;
    inline static int move_assignments = 0;

    int stored;

    explicit tracked_impl(int value)
    : stored(value) {
        ++live;
    }

    tracked_impl(tracked_impl const& other) noexcept
    : stored(other.stored) {
        ++live;
    }

    tracked_impl(tracked_impl&& other) noexcept
    : stored(std::exchange(other.stored, moved_from_value)) {
        ++live;
    }

    auto operator=(tracked_impl const& other) noexcept -> tracked_impl& {
        ++copy_assignments;
        stored = other.stored;
        return *this;
    }

    auto operator=(tracked_impl&& other) noexcept -> tracked_impl& {
        ++move_assignments;
        stored = std::exchange(other.stored, moved_from_value);
        return *this;
    }

    ~tracked_impl() {
        --live;
        ++destroyed;
    }

    auto value() const -> int {
        return stored;
    }

    static void reset() {
        live             = 0;
        destroyed        = 0;
        copy_assignments = 0;
        move_assignments = 0;
    }
};

using other_impl = value_impl<2>;
using variant_t  = trp::trait_variant<value_trait, tracked_impl, other_impl>;

void check_same_alternative() {
    tracked_impl::reset();
    {
        auto target      = variant_t(std::in_place_type<tracked_impl>, initial_value);
        auto copy_source = variant_t(std::in_place_type<tracked_impl>, copy_value);
        auto move_source = variant_t(std::in_place_type<tracked_impl>, move_value);

        target = copy_source;
        test::expect_eq(case_name, "same alternative copy value", target.value(), copy_value);
        test::expect_eq(case_name, "same alternative copy count", tracked_impl::copy_assignments, 1);
        test::expect_eq(case_name, "same alternative copy destroys nothing", tracked_impl::destroyed, 0);

        target = std::move(move_source);
        test::expect_eq(case_name, "same alternative move value", target.value(), move_value);
        test::expect_eq(case_name, "same alternative move count", tracked_impl::move_assignments, 1);
        test::expect_eq(case_name, "move source remains active", move_source.value(), moved_from_value);
        test::expect_eq(case_name, "same alternative move destroys nothing", tracked_impl::destroyed, 0);

        target = target;
        test::expect_eq(case_name, "self copy preserves value", target.value(), move_value);
        test::expect_eq(case_name, "self copy skips assignment", tracked_impl::copy_assignments, 1);

        target = std::move(target);
        test::expect_eq(case_name, "self move preserves value", target.value(), move_value);
        test::expect_eq(case_name, "self move skips assignment", tracked_impl::move_assignments, 1);
        test::expect_eq(case_name, "all alternatives remain live", tracked_impl::live, 3);
    }

    test::expect_eq(case_name, "all alternatives destroyed", tracked_impl::live, 0);
    test::expect_eq(case_name, "each alternative destroyed once", tracked_impl::destroyed, 3);
}

void check_replacement_destruction() {
    tracked_impl::reset();
    {
        auto value = variant_t(std::in_place_type<tracked_impl>, initial_value);

        value = other_impl{other_value};
        test::expect_eq(case_name, "replacement destroys old alternative", tracked_impl::destroyed, 1);
        test::expect_eq(case_name, "replacement clears tracked lifetime", tracked_impl::live, 0);

        (void)trp::emplace<tracked_impl>(value, recovered_value);
        test::expect_eq(case_name, "emplace starts tracked lifetime", tracked_impl::live, 1);
        test::expect_eq(case_name, "emplace preserves destruction count", tracked_impl::destroyed, 1);
    }

    test::expect_eq(case_name, "final tracked lifetime ended", tracked_impl::live, 0);
    test::expect_eq(case_name, "final tracked object destroyed once", tracked_impl::destroyed, 2);
}

inline void run() {
    check_same_alternative();
    check_replacement_destruction();
}

}    // namespace assignment
