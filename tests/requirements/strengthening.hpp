#pragma once

#include "requirements/support.hpp"

namespace strengthening {

inline constexpr auto case_name          = std::string_view("requirements.strengthening");
inline constexpr auto exact_result       = 61;
inline constexpr auto convertible_result = short{67};

struct relaxed_trait {
    auto refine(short value) -> int;
};

struct strenthened_trait : relaxed_trait {
    [[= trp::matching_return_signature]] auto refine(short value) -> int;
};

struct exact_ret_impl {
    auto refine(short) -> int {
        return exact_result;
    }
};

struct convert_ret_impl {
    auto refine(short) -> short {
        return convertible_result;
    }
};

struct exact_callable {
    auto operator()(short) -> int {
        return exact_result;
    }
};

struct convert_callable {
    auto operator()(short) -> short {
        return convertible_result;
    }
};

template<typename Callable>
struct callable_impl {
    Callable refine;
};

using exact_call   = callable_impl<exact_callable>;
using convert_call = callable_impl<convert_callable>;

static_assert(extract_first_req<strenthened_trait>().exact_return);
static_assert(!extract_first_req<strenthened_trait>().exact_args);

// clang-format off
static_assert(    trp::implements_trait<convert_ret_impl, relaxed_trait>);
static_assert(    trp::implements_trait<exact_ret_impl,   relaxed_trait>);
static_assert(    trp::implements_trait<convert_call,     relaxed_trait>);
static_assert(    trp::implements_trait<exact_call,       relaxed_trait>);

static_assert(    trp::implements_trait<exact_ret_impl,   strenthened_trait>);
static_assert(not trp::implements_trait<convert_ret_impl, strenthened_trait>);
static_assert(    trp::implements_trait<exact_call,       strenthened_trait>);
static_assert(not trp::implements_trait<convert_call,     strenthened_trait>);
// clang-format on

namespace combine {

struct strict_ret_trait {
    [[= trp::matching_return_signature]] auto score(short value) -> int;
};

struct strict_args_trait {
    [[= trp::matching_args_signature]] auto score(short value) -> int;
};

struct combined_trait
: strict_ret_trait
, strict_args_trait {
    // redeclaration with different signature requirements has no effect
    // because supertrait's requirements cannot be weakened
    [[= trp::relaxed_signature]] auto score(short value) -> int;
};

struct exact_score_impl {
    auto score(short) -> int {
        return exact_result;
    }
};

struct callable_impl {
    exact_callable score;
};

static_assert(trp::implements_trait<exact_score_impl, strict_ret_trait>);
static_assert(trp::implements_trait<exact_score_impl, strict_args_trait>);
static_assert(trp::implements_trait<exact_score_impl, combined_trait>);
static_assert(trp::implements_trait<callable_impl, strict_ret_trait>);
static_assert(trp::implements_trait<callable_impl, strict_args_trait>);
static_assert(trp::implements_trait<callable_impl, combined_trait>);
}    // namespace combine

void check_redeclared_strengthened_call() {
    auto impl = exact_ret_impl{};
    auto ref  = trp::dyn_trait_ref<strenthened_trait>(impl);

    test::expect_eq(case_name, "redeclared strict return", ref.refine(short_arg), exact_result);
}

void check_relaxed_base_call() {
    auto impl = convert_ret_impl{};
    auto ref  = trp::dyn_trait_ref<relaxed_trait>(impl);

    test::expect_eq(
        case_name, "relaxed base return conversion", ref.refine(short_arg), int{convertible_result});
}

void check_callable_calls() {
    auto exact     = exact_call{};
    auto exact_ref = trp::dyn_trait_ref<strenthened_trait>(exact);
    test::expect_eq(case_name, "callable strict return", exact_ref.refine(short_arg), exact_result);

    auto convert     = convert_call{};
    auto convert_ref = trp::dyn_trait_ref<relaxed_trait>(convert);
    test::expect_eq(
        case_name, "callable relaxed return", convert_ref.refine(short_arg), int{convertible_result});
}

inline void run() {
    check_redeclared_strengthened_call();
    check_relaxed_base_call();
    check_callable_calls();
}

}    // namespace strengthening
