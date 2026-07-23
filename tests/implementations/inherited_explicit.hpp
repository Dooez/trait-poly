#pragma once

#include "implementations/support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace inherited_explicit {

inline constexpr auto case_name = std::string_view("inherited_explicit_impl");
inline constexpr auto result    = 42;

struct base_impl {};
using derived_impl = derived<base_impl, 0>;

}    // namespace inherited_explicit

template<>
struct trp::impl_spec_for<inherited_explicit::base_impl, value_trait> {
    static auto value(auto&) -> int {
        return inherited_explicit::result;
    }
};

namespace inherited_explicit {

static_assert(trp::implements_trait<derived_impl, value_trait>);

inline void run() {
    auto impl = derived_impl{};
    auto ref  = trp::dyn_trait_ref<value_trait>(impl);
    test::expect_eq(case_name, "dyn ref inherited explicit implementation", ref.value(), result);

    auto variant = trp::trait_variant<value_trait, derived_impl>(std::in_place_type<derived_impl>);
    test::expect_eq(case_name, "variant inherited explicit implementation", variant.value(), result);
}

}    // namespace inherited_explicit
