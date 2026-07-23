#pragma once

#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace special_cases {

inline constexpr auto case_name               = std::string_view("overloads.special_cases");
inline constexpr auto argument                = short{4};
inline constexpr auto filtered_template_value = short{50};
inline constexpr auto selected_result         = 60;
inline constexpr auto sole_template_value     = short{70};
inline constexpr auto long_pick_result        = 10;
inline constexpr auto short_pick_result       = 20;
inline constexpr auto double_pick_result      = 30;

struct relaxed_trait {
    auto pick(short) -> int;
};
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

struct[[= trp::matching_return_signature]] matching_return_trait {
    auto pick(short) -> int;
};

struct return_disambiguated_impl {
    template<typename T>
    auto pick(T const&) -> short {
        return filtered_template_value;
    }

    auto pick(long) -> int {
        return selected_result;
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

struct sole_template_impl {
    template<typename T>
    auto pick(T const&) -> short {
        return sole_template_value;
    }
};

struct using_left_base {
    auto pick(int) -> int {
        return 1;
    }
};

struct using_right_base {
    auto pick(int) -> int {
        return 2;
    }
};

struct using_decl_impl
: using_left_base
, using_right_base {
    using using_right_base::pick;
};

// A normal overload set works with both relaxed and return-matching traits.
static_assert(trp::implements_trait<ordinary_impl, matching_return_trait>);
static_assert(trp::implements_trait<callable_impl, relaxed_trait>);
static_assert(trp::implements_trait<callable_impl, matching_return_trait>);

// C++26 reflection cannot resolve an overload set containing a function
// template, but matching the return type first removes that ambiguity.
static_assert(!trp::implements_trait<return_disambiguated_impl, relaxed_trait>);
static_assert(trp::implements_trait<return_disambiguated_impl, matching_return_trait>);

// No overload resolution is needed when the template is the only candidate.
static_assert(trp::implements_trait<sole_template_impl, relaxed_trait>);

// Native C++ sees the using declaration, but C++26 does not expose it through
// the reflection used for implementation discovery.
static_assert(requires(using_decl_impl impl, int value) { impl.pick(value); });
static_assert(!trp::implements_trait<using_decl_impl, relaxed_trait>);
static_assert(!trp::implements_trait<using_decl_impl, matching_return_trait>);

void check_return_disambiguation() {
    auto impl = return_disambiguated_impl{};
    auto view = trp::dyn_trait_ref<matching_return_trait>(impl);

    test::expect_eq(
        case_name, "return requirement selects ordinary overload", view.pick(argument), selected_result);
}

void check_callable_overload() {
    auto impl = callable_impl{};
    auto view = trp::dyn_trait_ref<matching_return_trait>(impl);

    test::expect_eq(case_name, "matching return callable overload", view.pick(argument), short_pick_result);
}

void check_sole_template() {
    auto impl = sole_template_impl{};
    auto view = trp::dyn_trait_ref<relaxed_trait>(impl);

    test::expect_eq(case_name, "sole template dispatch", view.pick(argument), int{sole_template_value});
}

inline void run() {
    check_return_disambiguation();
    check_callable_overload();
    check_sole_template();
}

}    // namespace special_cases
