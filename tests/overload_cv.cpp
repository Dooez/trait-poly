#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace {

inline constexpr auto case_name        = std::string_view("overload_cv");
inline constexpr auto const_result     = 1;
inline constexpr auto non_const_result = 2;
inline constexpr auto volatile_result  = 3;
inline constexpr auto cv_result        = 4;

struct value_trait {
    auto value() -> int;
    auto value() const -> int;
    auto value() volatile -> int;
    auto value() const volatile -> int;
};

struct overloaded_impl {
    auto value() const volatile -> int {
        return cv_result;
    }

    auto value() volatile -> int {
        return volatile_result;
    }

    auto value() const -> int {
        return const_result;
    }

    auto value() -> int {
        return non_const_result;
    }
};

static_assert(trp::implements_trait<overloaded_impl, value_trait>);

}    // namespace

int main() {
    auto impl = overloaded_impl{};
    auto ref  = trp::dyn_trait_ref<value_trait>(impl);
    test::expect_eq(case_name, "dyn ref mutable overload", ref.value(), non_const_result);

    auto const_ref = trp::dyn_trait_ref<value_trait const>(impl);
    test::expect_eq(case_name, "dyn ref const overload", const_ref.value(), const_result);

    auto volatile_ref = trp::dyn_trait_ref<value_trait volatile>(impl);
    test::expect_eq(case_name, "dyn ref volatile overload", volatile_ref.value(), volatile_result);

    auto cv_ref = trp::dyn_trait_ref<value_trait const volatile>(impl);
    test::expect_eq(case_name, "dyn ref cv overload", cv_ref.value(), cv_result);

    auto value = trp::trait_variant<value_trait, overloaded_impl>(std::in_place_type<overloaded_impl>);
    test::expect_eq(case_name, "variant mutable overload", value.value(), non_const_result);
    test::expect_eq(case_name, "variant const overload", std::as_const(value).value(), const_result);
    auto volatile&       volatile_value = value;
    auto const volatile& cv_value       = value;
    test::expect_eq(case_name, "variant volatile overload", volatile_value.value(), volatile_result);
    test::expect_eq(case_name, "variant cv overload", cv_value.value(), cv_result);
}
