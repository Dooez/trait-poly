#include "support/test_support.hpp"
#include "trp/trait_variant.hpp"

#include <string_view>
#include <utility>

namespace adaptertest {

inline constexpr auto case_name     = std::string_view("reference_adapters");
inline constexpr auto lvalue_result = 301;
inline constexpr auto rvalue_result = 302;

struct empty_impl {};

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

struct explicit_lvalue_trait {
    auto access() & -> int;
};

struct explicit_rvalue_trait {
    auto access() && -> int;
};

struct explicit_lvalue_impl {};
struct explicit_rvalue_impl {};

}    // namespace adaptertest

template<>
struct trp::impl_spec_for<adaptertest::explicit_lvalue_impl, adaptertest::explicit_lvalue_trait> {
    static auto access(auto&) -> int {
        return adaptertest::lvalue_result;
    }
};

template<>
struct trp::impl_spec_for<adaptertest::explicit_rvalue_impl, adaptertest::explicit_rvalue_trait> {
    static auto access(auto&&) -> int {
        return adaptertest::rvalue_result;
    }
};

namespace adaptertest {

static_assert(trp::implements_trait<empty_impl, lvalue_default_trait>);
static_assert(trp::implements_trait<empty_impl, rvalue_default_trait>);
static_assert(trp::implements_trait<explicit_lvalue_impl, explicit_lvalue_trait>);
static_assert(trp::implements_trait<explicit_rvalue_impl, explicit_rvalue_trait>);

void check_lvalue_defaults() {
    using variant = trp::trait_variant<lvalue_default_trait, empty_impl>;

    auto value = variant(std::in_place_type<empty_impl>);
    test::expect_eq(case_name, "inline lvalue default", value.access(), lvalue_result);
}

void check_lvalue_explicit_impl() {
    using variant = trp::trait_variant<explicit_lvalue_trait, explicit_lvalue_impl>;

    auto value = variant(std::in_place_type<explicit_lvalue_impl>);
    test::expect_eq(case_name, "explicit lvalue implementation", value.access(), lvalue_result);
}

void check_rvalue_defaults() {
    using variant = trp::trait_variant<rvalue_default_trait, empty_impl>;

    auto value = variant(std::in_place_type<empty_impl>);
    test::expect_eq(case_name, "inline rvalue default", std::move(value).access(), rvalue_result);
}

void check_rvalue_explicit_impl() {
    using variant = trp::trait_variant<explicit_rvalue_trait, explicit_rvalue_impl>;

    auto value = variant(std::in_place_type<explicit_rvalue_impl>);
    test::expect_eq(case_name, "explicit rvalue implementation", std::move(value).access(), rvalue_result);
}

}    // namespace adaptertest

int main() {
    adaptertest::check_lvalue_defaults();
    adaptertest::check_lvalue_explicit_impl();
    adaptertest::check_rvalue_defaults();
    adaptertest::check_rvalue_explicit_impl();

    return 0;
}
