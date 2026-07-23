#pragma once

#include "requirements/support.hpp"

namespace overrides {

inline constexpr auto case_name       = std::string_view("requirements.overrides");
inline constexpr auto relaxed_result  = short{73};
inline constexpr auto match_result    = 77;
inline constexpr auto exact_result    = 79;
inline constexpr auto exact_cv_result = 83;

template<typename Trait>
consteval auto requirement_for(std::string_view method_name) {
    constexpr auto methods = trp::detail::all_trait_methods_and_requirements<Trait>.identities;
    constexpr auto reqs    = trp::detail::all_trait_methods_and_requirements<Trait>.requirements;
    for (auto i = 0UZ; i < methods.size(); ++i)
        if (std::string_view(trp::detail::extract_method_identifier(methods[i])) == method_name)
            return reqs[i];
    throw "method requirement not found";
}

struct exact_cv_impl {
    auto implicit(short) -> int {
        return exact_cv_result;
    }

    auto value(short) -> int {
        return exact_cv_result;
    }
};

struct exact_impl {
    auto implicit(short) const -> int {
        return exact_result;
    }

    auto value(short) const -> int {
        return exact_result;
    }
};

struct match_ret_impl {
    auto implicit(short) const -> int {
        return match_result;
    }

    auto value(int) const -> int {
        return match_result;
    }
};

struct relaxed_impl {
    auto implicit(short) const -> int {
        return relaxed_result;
    }

    auto value(int) const -> short {
        return relaxed_result;
    }
};

struct default_trait {
    auto implicit(short value) -> int;

    auto value(short value) -> int;
};

struct single_relaxed_method_trait {
    [[= trp::relaxed_signature]] auto value(short value) -> int;
};

struct relaxed_method_trait {
    auto implicit(short value) -> int;

    [[= trp::relaxed_signature]] auto value(short value) -> int;
};

struct[[= trp::relaxed_signature]] relaxed_trait {
    auto implicit(short value) -> int;

    auto value(short value) -> int;
};

struct[[= trp::matching_return_signature]] match_return_trait {
    auto implicit(short value) -> int;

    auto value(short value) -> int;
};


// clang-format off
static_assert(    requirement_for<default_trait>("implicit").exact_return);
static_assert(    requirement_for<default_trait>("implicit").exact_args);
static_assert(    requirement_for<default_trait>("implicit").exact_cv);
static_assert(    requirement_for<default_trait>("value").exact_return);
static_assert(    requirement_for<default_trait>("value").exact_args);
static_assert(    requirement_for<default_trait>("value").exact_cv);

// accepts because default strict requirements are satisfied exactly.
static_assert(    trp::implements_trait<exact_cv_impl,  default_trait>);
// all fail because default requirements are exact_cv because of defines
static_assert(not trp::implements_trait<exact_impl,     default_trait>);
static_assert(not trp::implements_trait<match_ret_impl, default_trait>);
static_assert(not trp::implements_trait<relaxed_impl,   default_trait>);

static_assert(not requirement_for<relaxed_method_trait>("value").exact_return);
static_assert(not requirement_for<relaxed_method_trait>("value").exact_args);
static_assert(not requirement_for<relaxed_method_trait>("value").exact_cv);

// all pass because trait has only one method, and the method has annotation overriding defaults.
static_assert(    trp::implements_trait<exact_cv_impl,  single_relaxed_method_trait>);
static_assert(    trp::implements_trait<exact_impl,     single_relaxed_method_trait>);
static_assert(    trp::implements_trait<match_ret_impl, single_relaxed_method_trait>);
static_assert(    trp::implements_trait<relaxed_impl,   single_relaxed_method_trait>);

static_assert(    trp::implements_trait<exact_cv_impl,  relaxed_method_trait>);
// fail because only one of two methods has overriding annotation.
static_assert(not trp::implements_trait<exact_impl,     relaxed_method_trait>);
static_assert(not trp::implements_trait<match_ret_impl, relaxed_method_trait>);
static_assert(not trp::implements_trait<relaxed_impl,   relaxed_method_trait>);

static_assert(not requirement_for<relaxed_trait>("implicit").exact_return);
static_assert(not requirement_for<relaxed_trait>("implicit").exact_args);
static_assert(not requirement_for<relaxed_trait>("implicit").exact_cv);
static_assert(not requirement_for<relaxed_trait>("value").exact_return);
static_assert(not requirement_for<relaxed_trait>("value").exact_args);
static_assert(not requirement_for<relaxed_trait>("value").exact_cv);

// all pass because the trait-wide annotation overrides defaults.
static_assert(    trp::implements_trait<exact_cv_impl,  relaxed_trait>);
static_assert(    trp::implements_trait<exact_impl,     relaxed_trait>);
static_assert(    trp::implements_trait<match_ret_impl, relaxed_trait>);
static_assert(    trp::implements_trait<relaxed_impl,   relaxed_trait>);

static_assert(    requirement_for<match_return_trait>("implicit").exact_return);
static_assert(not requirement_for<match_return_trait>("implicit").exact_args);
static_assert(not requirement_for<match_return_trait>("implicit").exact_cv);
static_assert(    requirement_for<match_return_trait>("value").exact_return);
static_assert(not requirement_for<match_return_trait>("value").exact_args);
static_assert(not requirement_for<match_return_trait>("value").exact_cv);

// pass because the trait-wide annotation requires only exact return.
static_assert(    trp::implements_trait<exact_cv_impl,  match_return_trait>);
static_assert(    trp::implements_trait<exact_impl,     match_return_trait>);
static_assert(    trp::implements_trait<match_ret_impl, match_return_trait>);
// fails because exact return is still required.
static_assert(not trp::implements_trait<relaxed_impl,   match_return_trait>);

// default, trait, and method requirements are non-additive and all differ here:
struct[[= trp::matching_return_signature]] non_additive_trait {
    [[= trp::matching_args_signature]] auto value(short value) -> int;
};
// matching argument, but converting return
struct marg_cret_impl {
    auto value(short) const -> short {
        return match_result;
    }
};

// default exact_cv = return + args + cv, implicit uses trait = return only, value uses method = args only.
static_assert(not requirement_for<non_additive_trait>("value").exact_return);
static_assert(    requirement_for<non_additive_trait>("value").exact_args);
static_assert(not requirement_for<non_additive_trait>("value").exact_cv);

// pass because the method annotation requires only exact arguments.
static_assert(    trp::implements_trait<exact_cv_impl,  non_additive_trait>);
static_assert(    trp::implements_trait<exact_impl,     non_additive_trait>);
// pass despite converting return.
static_assert(    trp::implements_trait<marg_cret_impl, non_additive_trait>);
// fail because exact arguments are required.
static_assert(not trp::implements_trait<relaxed_impl,   non_additive_trait>);
static_assert(not trp::implements_trait<match_ret_impl, non_additive_trait>);
// clang-format on

void check_relaxed_impl() {
    auto impl = relaxed_impl{};
    auto ref  = trp::dyn_trait_ref<relaxed_trait>(impl);

    test::expect_eq(case_name, "relaxed impl", ref.value(short_arg), int{relaxed_result});
}

void check_match_return_impl() {
    auto impl = match_ret_impl{};
    auto ref  = trp::dyn_trait_ref<match_return_trait>(impl);

    test::expect_eq(case_name, "match return impl", ref.value(short_arg), match_result);
}

void check_exact_impl() {
    auto impl = exact_impl{};
    auto ref  = trp::dyn_trait_ref<match_return_trait>(impl);

    test::expect_eq(case_name, "exact impl", ref.value(short_arg), exact_result);
}

void check_exact_cv_impl() {
    auto impl = exact_cv_impl{};
    auto ref  = trp::dyn_trait_ref<default_trait>(impl);

    test::expect_eq(case_name, "exact cv impl", ref.value(short_arg), exact_cv_result);
}

void check_non_additive_overrides() {
    auto impl = exact_impl{};
    auto ref  = trp::dyn_trait_ref<non_additive_trait>(impl);

    test::expect_eq(case_name, "method-level override", ref.value(short_arg), exact_result);
}

inline void run() {
    check_relaxed_impl();
    check_match_return_impl();
    check_exact_impl();
    check_exact_cv_impl();
    check_non_additive_overrides();
}

}    // namespace overrides
