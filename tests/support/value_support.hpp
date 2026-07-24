#pragma once

namespace valuetest {

inline constexpr auto moved_from_value = -1;

struct trait {
    auto value() const -> int;
};

struct mutable_trait {
    auto value() -> int;
};

template<int>
struct value_impl {
    int stored;

    explicit value_impl(int value)
    : stored(value) {}

    auto value() const -> int {
        return stored;
    }
};

template<int>
struct marker {};

struct construction_error {};
struct copy_error {};
struct move_error {};

struct throwing_impl {
    inline static bool fail_construction = false;
    inline static bool fail_copy         = false;
    inline static bool fail_move         = false;

    int stored;

    explicit throwing_impl(int value)
    : stored(value) {
        if (fail_construction)
            throw construction_error{};
    }

    throwing_impl(throwing_impl const& other)
    : stored(other.stored) {
        if (fail_copy)
            throw copy_error{};
    }

    throwing_impl(throwing_impl&& other)
    : stored(other.stored) {
        if (fail_move)
            throw move_error{};
        other.stored = moved_from_value;
    }

    auto value() const -> int {
        return stored;
    }

    static void reset() {
        fail_construction = false;
        fail_copy         = false;
        fail_move         = false;
    }
};

struct alignas(128) aligned_impl {
    int stored;

    explicit aligned_impl(int value)
    : stored(value) {}

    auto value() const -> int {
        return stored;
    }
};

}    // namespace valuetest
