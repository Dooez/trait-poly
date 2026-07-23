#pragma once

#include <type_traits>

namespace reftest {

inline constexpr auto unqualified_result     = 10;
inline constexpr auto lvalue_result          = 20;
inline constexpr auto rvalue_result          = 30;
inline constexpr auto const_lvalue_result    = 40;
inline constexpr auto const_rvalue_result    = 50;
inline constexpr auto volatile_lvalue_result = 60;
inline constexpr auto volatile_rvalue_result = 70;
inline constexpr auto cv_lvalue_result       = 80;
inline constexpr auto cv_rvalue_result       = 90;

struct unqualified_trait {
    auto access() -> int;
};

struct lvalue_trait {
    auto access() & -> int;
};

struct rvalue_trait {
    auto access() && -> int;
};

struct split_trait {
    auto access() & -> int;
    auto access() && -> int;
};

struct unqualified_impl {
    auto access() -> int {
        return unqualified_result;
    }
};

struct lvalue_impl {
    auto access() & -> int {
        return lvalue_result;
    }
};

struct rvalue_impl {
    auto access() && -> int {
        return rvalue_result;
    }
};

struct split_impl {
    auto access() & noexcept -> int {
        return lvalue_result;
    }

    auto access() && -> int {
        return rvalue_result;
    }
};

struct eop_split_impl {
    auto access(this eop_split_impl&) -> int {
        return lvalue_result;
    }

    auto access(this eop_split_impl&&) -> int {
        return rvalue_result;
    }
};

struct forwarding_impl {
    auto access(this auto&& self) -> int {
        if constexpr (std::is_lvalue_reference_v<decltype(self)>)
            return lvalue_result;
        return rvalue_result;
    }
};

struct callable_impl {
    struct ref_callable {
        auto operator()() & -> int {
            return lvalue_result;
        }

        auto operator()() && -> int {
            return rvalue_result;
        }
    };

    ref_callable access;
};

struct cvref_trait {
    auto access() & -> int;
    auto access() const& -> int;
    auto access() volatile& -> int;
    auto access() const volatile& -> int;
    auto access() && -> int;
    auto access() const&& -> int;
    auto access() volatile&& -> int;
    auto access() const volatile&& -> int;
};

struct cvref_impl {
    auto access() & -> int {
        return lvalue_result;
    }

    auto access() const& -> int {
        return const_lvalue_result;
    }

    auto access() volatile& -> int {
        return volatile_lvalue_result;
    }

    auto access() const volatile& -> int {
        return cv_lvalue_result;
    }

    auto access() && -> int {
        return rvalue_result;
    }

    auto access() const&& -> int {
        return const_rvalue_result;
    }

    auto access() volatile&& -> int {
        return volatile_rvalue_result;
    }

    auto access() const volatile&& -> int {
        return cv_rvalue_result;
    }
};

}    // namespace reftest
