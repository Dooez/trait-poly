#pragma once

#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace cv {

inline constexpr auto case_name        = std::string_view("overloads.cv");
inline constexpr auto eop_case_name    = std::string_view("overloads.cv.eop");
inline constexpr auto const_result     = 1;
inline constexpr auto non_const_result = 2;
inline constexpr auto volatile_result  = 3;
inline constexpr auto cv_result        = 4;

struct trait {
    auto pick() -> int;
    auto pick() const -> int;
    auto pick() volatile -> int;
    auto pick() const volatile -> int;
};

struct impl {
    auto pick() const volatile -> int {
        return cv_result;
    }

    auto pick() volatile -> int {
        return volatile_result;
    }

    auto pick() const -> int {
        return const_result;
    }

    auto pick() -> int {
        return non_const_result;
    }
};

struct eop_impl {
    auto pick(this eop_impl const volatile&) -> int {
        return cv_result;
    }
    auto pick(this eop_impl volatile&) -> int {
        return volatile_result;
    }
#ifndef __clang__
    // clang erroneously rejects volatile and non-volatile as redeclarations
    auto pick(this eop_impl const&) -> int {
        return const_result;
    }

    auto pick(this eop_impl&) -> int {
        return non_const_result;
    }
#endif
};

static_assert(trp::implements_trait<impl, trait>);
static_assert(trp::implements_trait<eop_impl, trait>);

template<typename Impl>
void check_dispatch(std::string_view context) {
    auto object = Impl{};
    auto ref    = trp::dyn_trait_ref<trait>(object);
    test::expect_eq(context, "dyn ref mutable overload", ref.pick(), non_const_result);

    auto const_ref = trp::dyn_trait_ref<trait const>(object);
    test::expect_eq(context, "dyn ref const overload", const_ref.pick(), const_result);

    auto volatile_ref = trp::dyn_trait_ref<trait volatile>(object);
    test::expect_eq(context, "dyn ref volatile overload", volatile_ref.pick(), volatile_result);

    auto cv_ref = trp::dyn_trait_ref<trait const volatile>(object);
    test::expect_eq(context, "dyn ref cv overload", cv_ref.pick(), cv_result);

    auto value = trp::trait_variant<trait, Impl>(std::in_place_type<Impl>);
    test::expect_eq(context, "variant mutable overload", value.pick(), non_const_result);
    test::expect_eq(context, "variant const overload", std::as_const(value).pick(), const_result);
    auto volatile&       volatile_value = value;
    auto const volatile& cv_value       = value;
    test::expect_eq(context, "variant volatile overload", volatile_value.pick(), volatile_result);
    test::expect_eq(context, "variant cv overload", cv_value.pick(), cv_result);
}

inline void run() {
    check_dispatch<impl>(case_name);
    check_dispatch<eop_impl>(eop_case_name);
}

}    // namespace cv
