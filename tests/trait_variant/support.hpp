#pragma once

struct value_trait {
    auto value() const -> int;
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
