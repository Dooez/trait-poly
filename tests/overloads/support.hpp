#pragma once

namespace overloadtest {

inline constexpr auto long_pick_result   = 10;
inline constexpr auto short_pick_result  = 20;
inline constexpr auto double_pick_result = 30;

struct ordinary_impl {
    auto pick(long) -> int {
        return long_pick_result;
    }

    auto pick(int) -> int {
        return short_pick_result;
    }

    auto pick(double) -> int {
        return double_pick_result;
    }
};

struct callable_impl {
    struct callable {
        auto operator()(long) -> int {
            return long_pick_result;
        }

        auto operator()(int) -> int {
            return short_pick_result;
        }

        auto operator()(double) -> int {
            return double_pick_result;
        }
    };

    callable pick;
};

}    // namespace overloadtest
