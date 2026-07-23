#pragma once

#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace ref {

inline constexpr auto case_name     = std::string_view("overloads.ref");
inline constexpr auto eop_case_name = std::string_view("overloads.ref.eop");
inline constexpr auto lvalue_result = 1;
inline constexpr auto rvalue_result = 2;

struct trait {
    auto pick() & -> int;
    auto pick() && -> int;
};

struct impl {
    auto pick() const& -> int {
        return -1;
    }

    auto pick() & -> int {
        return lvalue_result;
    }

    auto pick() const&& -> int {
        return -2;
    }

    auto pick() && -> int {
        return rvalue_result;
    }
};

struct eop_impl {
    auto pick(this eop_impl const&) -> int {
        return -1;
    }

    auto pick(this eop_impl&) -> int {
        return lvalue_result;
    }

    auto pick(this eop_impl const&&) -> int {
        return -2;
    }

    auto pick(this eop_impl&&) -> int {
        return rvalue_result;
    }
};

static_assert(trp::implements_trait<impl, trait>);
static_assert(trp::implements_trait<eop_impl, trait>);

template<typename Impl>
void check_dispatch(std::string_view context) {
    auto object = Impl{};
    auto view   = trp::dyn_trait_ref<trait>(object);
    test::expect_eq(context, "dyn ref lvalue overload", view.pick(), lvalue_result);

    auto value = trp::trait_variant<trait, Impl>(std::in_place_type<Impl>);
    test::expect_eq(context, "variant lvalue overload", value.pick(), lvalue_result);
    test::expect_eq(context, "variant rvalue overload", std::move(value).pick(), rvalue_result);
}

inline void run() {
    check_dispatch<impl>(case_name);
    check_dispatch<eop_impl>(eop_case_name);
}

}    // namespace ref
