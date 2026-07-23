#pragma once

#include "dyn_trait_ref/support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

#include <utility>

namespace casts {

inline constexpr auto writable_value       = 11;
inline constexpr auto prechecked_value     = 23;
inline constexpr auto other_readable_value = 99;
inline constexpr auto other_writable_value = 77;
inline constexpr auto case_name            = std::string_view("dyn_trait_ref");

struct readable_trait {
    auto read() const -> int;
};

struct writable_trait : readable_trait {
    auto write(int value) noexcept -> void;
};

struct writable_impl {
    int value = writable_value;

    auto read() const -> int {
        return value;
    }

    auto write(int new_value) noexcept -> void {
        value = new_value;
    }
};

using writable_ref = trp::dyn_trait_ref<writable_trait>;

template<typename Arg>
concept can_write = requires(writable_ref& ref, Arg&& arg) { ref.write(static_cast<Arg&&>(arg)); };

static_assert(can_write<int>);
static_assert(!can_write<char const*>);
static_assert(noexcept(std::declval<writable_ref&>().write(1)));

struct other_readable_impl {
    auto read() const -> int {
        return other_readable_value;
    }
};

struct other_writable_impl {
    auto read() const -> int {
        return other_writable_value;
    }

    auto write(int) noexcept -> void {}
};

void check_const_trait_cast_precheck() {
    auto impl      = unq_impl{};
    auto const_ref = trp::dyn_trait_ref<full_trait const>(impl);

    test::expect_eq(case_name,
                    "unq impl const cast is valid",
                    trp::is_valid_const_trait_cast<full_trait>(const_ref),
                    true);

    auto unq_ref = trp::const_trait_cast<full_trait>(const_ref);
    test::expect_eq(case_name, "checked const trait cast reaches unq method", unq_ref.unq(), unq_result);

    auto readonly_impl = c_impl{};
    auto readonly_ref  = trp::dyn_trait_ref<full_trait const>(readonly_impl);
    test::expect_eq(case_name,
                    "const-only impl const cast is rejected",
                    trp::is_valid_const_trait_cast<full_trait>(readonly_ref),
                    false);
    test::expect_eq(case_name, "const-only ref still reads", readonly_ref.c(), c_result);
}

void check_type_checks_and_trait_casts() {
    auto impl = writable_impl{};
    auto ref  = writable_ref(impl);

    test::expect_eq(case_name, "holding exact writable impl", trp::is_holding_type<writable_impl>(ref), true);
    test::expect_eq(
        case_name, "not holding other impl", trp::is_holding_type<other_writable_impl>(ref), false);

    auto readable_ref = trp::trait_cast<readable_trait>(ref);
    test::expect_eq(case_name, "explicit supertrait reads", readable_ref.read(), writable_value);
    test::expect_eq(
        case_name, "supertrait keeps runtime type", trp::is_holding_type<writable_impl>(readable_ref), true);

    if (trp::is_holding_type<writable_impl>(readable_ref)) {
        auto writable_ref = trp::trait_cast<writable_trait, writable_impl>(readable_ref);
        writable_ref.write(prechecked_value);
    }
    test::expect_eq(case_name, "prechecked dyn trait cast writes", impl.value, prechecked_value);

    auto other_impl = other_readable_impl{};
    auto other_ref  = trp::dyn_trait_ref<readable_trait>(other_impl);
    test::expect_eq(case_name,
                    "wrong impl precheck rejects unsafe cast",
                    trp::is_holding_type<writable_impl>(other_ref),
                    false);
    test::expect_eq(case_name, "wrong impl remains readable", other_ref.read(), other_readable_value);
}

inline void run() {
    check_const_trait_cast_precheck();
    check_type_checks_and_trait_casts();
}

}    // namespace casts
