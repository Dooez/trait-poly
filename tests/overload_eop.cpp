#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace {

inline constexpr auto case_name      = std::string_view("overload_eop");
inline constexpr auto int_result     = 2;
inline constexpr auto mutable_result = 4;
inline constexpr auto rvalue_result  = 6;

struct value_trait {
    auto value(short) -> int;
    auto qualified(short) -> int;
    auto consumed(short) && -> int;
};

struct overloaded_impl {
    auto value(this overloaded_impl&, long) -> int {
        return 1;
    }

    auto value(this overloaded_impl&, int) -> int {
        return int_result;
    }

    auto value(this overloaded_impl&, double) -> int {
        return 3;
    }

    auto qualified(this overloaded_impl const&, int) -> int {
        return 3;
    }

    auto qualified(this overloaded_impl&, int) -> int {
        return mutable_result;
    }

    auto consumed(this overloaded_impl const&&, int) -> int {
        return 5;
    }

    auto consumed(this overloaded_impl&&, int) -> int {
        return rvalue_result;
    }
};

static_assert(trp::implements_trait<overloaded_impl, value_trait>);

}    // namespace

int main() {
    auto impl = overloaded_impl{};
    auto ref  = trp::dyn_trait_ref<value_trait>(impl);
    test::expect_eq(case_name, "dyn ref explicit-object overload", ref.value(0), int_result);
    test::expect_eq(case_name, "dyn ref explicit-object cv overload", ref.qualified(0), mutable_result);

    auto value = trp::trait_variant<value_trait, overloaded_impl>(std::in_place_type<overloaded_impl>);
    test::expect_eq(case_name, "variant explicit-object overload", value.value(0), int_result);
    test::expect_eq(case_name, "variant explicit-object cv overload", value.qualified(0), mutable_result);
    test::expect_eq(
        case_name, "variant explicit-object ref overload", std::move(value).consumed(0), rvalue_result);
}
