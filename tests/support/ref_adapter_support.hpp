#pragma once

#include "support/ref_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace adapters {

inline constexpr auto lvalue_result = 301;
inline constexpr auto rvalue_result = 302;

struct impl {};

struct lvalue_default_trait {
    auto access() & -> int;

    static auto access(auto&) -> int {
        return lvalue_result;
    }
};

struct rvalue_default_trait {
    auto access() && -> int;

    static auto access(auto&&) -> int {
        return rvalue_result;
    }
};

}    // namespace adapters

template<>
struct trp::impl_spec_for<adapters::impl, reftest::lvalue_trait> {
    static auto access(auto&) -> int {
        return adapters::lvalue_result;
    }
};

template<>
struct trp::impl_spec_for<adapters::impl, reftest::rvalue_trait> {
    static auto access(auto&&) -> int {
        return adapters::rvalue_result;
    }
};
