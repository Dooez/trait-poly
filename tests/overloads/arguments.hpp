#pragma once

#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace arguments {

inline constexpr auto case_name  = std::string_view("overloads.arguments");
inline constexpr auto short_arg  = short{1};
inline constexpr auto double_arg = double{1};

inline constexpr auto long_pick_result   = 10;
inline constexpr auto short_pick_result  = 20;
inline constexpr auto double_pick_result = 30;

struct trait {
    auto pick(short) -> int;
    auto pick(double) -> int;
};
using ref = trp::dyn_trait_ref<trait>;

template<typename Impl>
using variant = trp::trait_variant<trait, Impl>;

struct pick_impl {
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

struct defaulted_impl {
    auto pick(long, int = 0) -> int {
        return long_pick_result;
    }

    auto pick(short, int = 0) -> int {
        return short_pick_result;
    }

    auto pick(double, int = 0) -> int {
        return double_pick_result;
    }
};

struct eop_impl {
    auto pick(this eop_impl&, long) -> int {
        return long_pick_result;
    }

    auto pick(this eop_impl&, short) -> int {
        return short_pick_result;
    }

    auto pick(this eop_impl&, double) -> int {
        return double_pick_result;
    }
};

struct static_impl {
    static auto pick(long) -> int {
        return long_pick_result;
    }

    static auto pick(short) -> int {
        return short_pick_result;
    }

    static auto pick(double) -> int {
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

static_assert(trp::implements_trait<pick_impl, trait>);
static_assert(trp::implements_trait<defaulted_impl, trait>);
static_assert(trp::implements_trait<eop_impl, trait>);
static_assert(trp::implements_trait<static_impl, trait>);
static_assert(trp::implements_trait<callable_impl, trait>);

template<typename Impl>
void check_dispatch(std::string_view ref_check, std::string_view variant_check) {
    auto impl      = Impl{};
    auto trait_ref = ref(impl);
    test::expect_eq(case_name, ref_check, trait_ref.pick(short_arg), short_pick_result);
    test::expect_eq(case_name, ref_check, trait_ref.pick(double_arg), double_pick_result);

    auto value = variant<Impl>(std::in_place_type<Impl>);
    test::expect_eq(case_name, variant_check, value.pick(short_arg), short_pick_result);
    test::expect_eq(case_name, variant_check, value.pick(double_arg), double_pick_result);
}

inline void run() {
    check_dispatch<pick_impl>("dyn ref ordinary overload", "variant ordinary overload");
    check_dispatch<defaulted_impl>("dyn ref defaulted overload", "variant defaulted overload");
    check_dispatch<eop_impl>("dyn ref explicit-object overload", "variant explicit-object overload");
    check_dispatch<static_impl>("dyn ref static overload", "variant static overload");
    check_dispatch<callable_impl>("dyn ref callable overload", "variant callable overload");
}

}    // namespace arguments
