#pragma once

#include "implementations/support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace inherited_virtual {

inline constexpr auto case_name = std::string_view("inherited_virtual_impl");
inline constexpr auto result    = 37;

struct base_impl {
    auto value() -> int {
        return result;
    }
};

using virtual_left  = virtual_derived<base_impl, 0>;
using virtual_right = virtual_derived<base_impl, 1>;
struct virtual_impl
: virtual_left
, virtual_right {};

using nonvirtual_left  = derived<base_impl, 0>;
using nonvirtual_right = derived<base_impl, 1>;
struct ambiguous_impl
: nonvirtual_left
, nonvirtual_right {};

template<typename T>
concept can_call_value = requires(T& value) { value.value(); };

static_assert(can_call_value<virtual_impl>);
static_assert(trp::implements_trait<virtual_impl, value_trait>);

static_assert(!can_call_value<ambiguous_impl>);
static_assert(!trp::implements_trait<ambiguous_impl, value_trait>);

inline void run() {
    auto impl = virtual_impl{};
    auto ref  = trp::dyn_trait_ref<value_trait>(impl);
    auto var  = trp::trait_variant<value_trait, virtual_impl>(std::in_place_type<virtual_impl>);

    test::expect_eq(case_name, "dyn ref virtual base dispatch", ref.value(), result);
    test::expect_eq(case_name, "variant virtual base dispatch", var.value(), result);
}

}    // namespace inherited_virtual
