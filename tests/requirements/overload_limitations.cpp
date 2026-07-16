#include "support/requirement_support.hpp"

namespace {

inline constexpr auto case_name             = std::string_view("requirements.overload_limitations");
inline constexpr auto normal_int_result     = 41;
inline constexpr auto normal_long_result    = 43;
inline constexpr auto using_right_result    = 47;
inline constexpr auto using_left_result     = 53;
inline constexpr auto template_limit_result = 59;
inline constexpr auto selected_result       = 61;
inline constexpr auto template_result       = short{67};
inline constexpr auto variant_arg           = short{4};
inline constexpr auto trait_index           = 0;

struct[[= trp::matching_return_signature]] trait {
    auto pick(short value) -> int;
};

struct relaxed_trait {
    auto pick(short value) -> int;
};

struct normal_overloads_impl {
    auto pick(int) -> int {
        return normal_int_result;
    }

    auto pick(long) -> int {
        return normal_long_result;
    }
};

struct template_impl {
    template<typename T>
    auto pick(T const&) -> short {
        return template_limit_result;
    }

    auto pick(long) -> int {
        return template_limit_result;
    }
};

struct using_left_base {
    auto pick(int) -> int {
        return using_left_result;
    }
};

struct using_right_base {
    auto pick(int) -> int {
        return using_right_result;
    }
};

struct using_decl_impl
: using_left_base
, using_right_base {
    using using_right_base::pick;
};

// clang-format off
// accepts because ordinary non-template overload resolution can be performed.
static_assert(trp::implements_trait<normal_overloads_impl, relaxed_trait>);
static_assert(trp::implements_trait<normal_overloads_impl, trait>);

// fails because overload set that includes template(s) cannot be resolved in C++26.
static_assert(not trp::implements_trait<template_impl, relaxed_trait>);
// accepts because strict return discards the convertible template before overload resolution.
static_assert(    trp::implements_trait<template_impl, trait>);
// need to add test where template can be resolved exactly

// native C++ sees the using declaration.
static_assert(requires(using_decl_impl impl, int v) { impl.pick(v); });
// fails because C++26 does not support using declaration reflections
static_assert(not trp::implements_trait<using_decl_impl, relaxed_trait>);
static_assert(not trp::implements_trait<using_decl_impl, trait>);

// clang-format on
}    // namespace

int main() {
    return reqtest::success_exit_code;
}
