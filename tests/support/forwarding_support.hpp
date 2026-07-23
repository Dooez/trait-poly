#pragma once

#include <concepts>
#include <utility>

namespace forwardtest {

inline constexpr auto initial_value     = 1;
inline constexpr auto changed_value     = 2;
inline constexpr auto move_only_value   = 3;
inline constexpr auto immovable_value   = 4;
inline constexpr auto lvalue_argument   = 5;
inline constexpr auto lvalue_forwarded  = 6;
inline constexpr auto by_value_argument = 7;
inline constexpr auto rvalue_argument   = 8;
inline constexpr auto rvalue_forwarded  = 9;
inline constexpr auto moved_from_value  = -1;

struct reference_trait {
    auto mutable_value() -> int&;
    auto const_value() const -> int const&;
};

struct reference_impl {
    int stored = initial_value;

    auto mutable_value() -> int& {
        return stored;
    }

    auto const_value() const -> int const& {
        return stored;
    }
};

struct move_only_type {
    int value;

    explicit move_only_type(int initial)
    : value(initial) {}

    move_only_type(move_only_type const&) = delete;

    move_only_type(move_only_type&& other) noexcept
    : value(std::exchange(other.value, moved_from_value)) {}

    auto operator=(move_only_type const&) -> move_only_type& = delete;

    auto operator=(move_only_type&& other) noexcept -> move_only_type& {
        value = std::exchange(other.value, moved_from_value);
        return *this;
    }
};

static_assert(std::movable<move_only_type>);
static_assert(not std::copy_constructible<move_only_type>);

struct immovable_type {
    int value;

    explicit immovable_type(int initial)
    : value(initial) {}

    immovable_type(immovable_type const&) = delete;
    immovable_type(immovable_type&&)      = delete;
};

struct prvalue_trait {
    auto make_move_only() -> move_only_type;
    auto make_immovable() -> immovable_type;
};

struct prvalue_impl {
    auto make_move_only() -> move_only_type {
        return move_only_type(move_only_value);
    }

    auto make_immovable() -> immovable_type {
        return immovable_type(immovable_value);
    }
};

struct argument_trait {
    auto mutate(int&) -> void;
    auto take_value(move_only_type) -> void;
    auto take_rvalue(move_only_type&&) -> void;
};

struct argument_impl {
    int             by_value       = 0;
    int             rvalue         = 0;
    int*            lvalue_address = nullptr;
    move_only_type* rvalue_address = nullptr;

    auto mutate(int& argument) -> void {
        lvalue_address = &argument;
        argument       = lvalue_forwarded;
    }

    auto take_value(move_only_type argument) -> void {
        by_value = argument.value;
    }

    auto take_rvalue(move_only_type&& argument) -> void {
        rvalue         = argument.value;
        rvalue_address = &argument;
        argument.value = rvalue_forwarded;
    }
};

}    // namespace forwardtest
