#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

inline constexpr auto pick_long_result    = 10;
inline constexpr auto pick_int_result     = 20;
inline constexpr auto pick_double_result  = 30;
inline constexpr auto exact_long_result   = 40;
inline constexpr auto exact_int_result    = 50;
inline constexpr auto pick_arg            = 1;
inline constexpr auto exact_arg           = 1;
inline constexpr auto eop_initial_value   = 7;
inline constexpr auto eop_ref_value       = 9;
inline constexpr auto eop_variant_initial = 11;
inline constexpr auto eop_variant_value   = 13;
inline constexpr auto success_exit_code   = 0;
inline constexpr auto case_name           = std::string_view("overload_resolution");

struct arithmetic_trait {
    auto pick(short value) -> int;
    auto exact(int value) -> int;
};

struct arithmetic_impl {
    auto pick(long) -> int {
        return pick_long_result;
    }

    auto pick(int) -> int {
        return pick_int_result;
    }

    auto pick(double) -> int {
        return pick_double_result;
    }

    auto exact(long) -> int {
        return exact_long_result;
    }

    auto exact(int) -> int {
        return exact_int_result;
    }
};

struct eop_trait {
    auto value() const -> int;
    auto set(int value) -> void;
};

struct eop_impl {
    int stored = 0;

    auto value(this eop_impl const& self) -> int {
        return self.stored;
    }

    auto set(this eop_impl& self, int value) -> void {
        self.stored = value;
    }
};

using arithmetic_variant = trp::trait_variant<arithmetic_trait, arithmetic_impl>;
using eop_variant        = trp::trait_variant<eop_trait, eop_impl>;

static_assert(trp::implements_trait<arithmetic_impl, arithmetic_trait>);
static_assert(trp::implements_trait<eop_impl, eop_trait>);

void check_dyn_ref_overloads() {
    auto impl = arithmetic_impl{};
    auto ref  = trp::dyn_trait_ref<arithmetic_trait>(impl);

    test::expect_eq(case_name, "promotion overload selected", ref.pick(pick_arg), pick_int_result);
    test::expect_eq(case_name, "exact overload selected", ref.exact(exact_arg), exact_int_result);
}

void check_variant_overloads() {
    auto var = arithmetic_variant(std::in_place_type<arithmetic_impl>);

    test::expect_eq(case_name, "variant promotion overload", var.pick(pick_arg), pick_int_result);
    test::expect_eq(case_name, "variant exact overload", var.exact(exact_arg), exact_int_result);
}

void check_explicit_object_methods() {
    auto impl = eop_impl{.stored = eop_initial_value};
    auto ref  = trp::dyn_trait_ref<eop_trait>(impl);

    test::expect_eq(case_name, "eop ref read", ref.value(), eop_initial_value);
    ref.set(eop_ref_value);
    test::expect_eq(case_name, "eop ref write", impl.stored, eop_ref_value);

    auto var = eop_variant(std::in_place_type<eop_impl>, eop_variant_initial);
    test::expect_eq(case_name, "eop variant read", var.value(), eop_variant_initial);
    var.set(eop_variant_value);
    test::expect_eq(case_name, "eop variant write", var.value(), eop_variant_value);
}

int main() {
    check_dyn_ref_overloads();
    check_variant_overloads();
    check_explicit_object_methods();

    return success_exit_code;
}
