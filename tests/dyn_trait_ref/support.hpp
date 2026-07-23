#pragma once

#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

// Shared CV fixtures for this test executable.
inline constexpr auto unq_result = 1;
inline constexpr auto c_result   = 2;
inline constexpr auto v_result   = 3;
inline constexpr auto cv_result  = 4;

struct full_trait {
    auto unq() -> int;
    auto c() const -> int;
    auto v() volatile -> int;
    auto cv() const volatile -> int;
};

struct unq_impl {
    auto unq() -> int {
        return unq_result;
    }

    auto c() const -> int {
        return c_result;
    }

    auto v() volatile -> int {
        return v_result;
    }

    auto cv() const volatile -> int {
        return cv_result;
    }
};

struct c_impl {
    auto c() const -> int {
        return c_result;
    }

    auto cv() const volatile -> int {
        return cv_result;
    }
};

struct v_impl {
    auto v() volatile -> int {
        return v_result;
    }

    auto cv() const volatile -> int {
        return cv_result;
    }
};

struct cv_impl {
    auto cv() const volatile -> int {
        return cv_result;
    }
};

template<typename T>
concept can_unq = requires(T obj) { obj.unq(); };

template<typename T>
concept can_c = requires(T obj) { obj.c(); };

template<typename T>
concept can_v = requires(T obj) { obj.v(); };

template<typename T>
concept can_cv = requires(T obj) { obj.cv(); };

using ref_unq = trp::dyn_trait_ref<full_trait>;
using ref_c   = trp::dyn_trait_ref<full_trait const>;
using ref_v   = trp::dyn_trait_ref<full_trait volatile>;
using ref_cv  = trp::dyn_trait_ref<full_trait const volatile>;

using var_unq = trp::trait_variant<full_trait, unq_impl>;
using var_c   = trp::trait_variant<full_trait const, unq_impl>;
using var_v   = trp::trait_variant<full_trait volatile, unq_impl>;
using var_cv  = trp::trait_variant<full_trait const volatile, unq_impl>;
