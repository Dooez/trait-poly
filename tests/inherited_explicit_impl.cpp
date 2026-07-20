#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace inherited {

inline constexpr auto case_name = std::string_view("inherited_explicit_impl");
inline constexpr auto result    = 42;

struct value_trait {
    auto value() -> int;
};

struct base_impl {};
struct derived_impl : base_impl {};

}    // namespace inherited

template<>
struct trp::impl_spec_for<inherited::base_impl, inherited::value_trait> {
    static auto value(auto&) -> int {
        return inherited::result;
    }
};

namespace inherited {

static_assert(trp::implements_trait<derived_impl, value_trait>);

}    // namespace inherited

int main() {
    using namespace inherited;

    auto impl = derived_impl{};
    auto ref  = trp::dyn_trait_ref<value_trait>(impl);
    test::expect_eq(case_name, "dyn ref inherited explicit implementation", ref.value(), result);

    auto value = trp::trait_variant<value_trait, derived_impl>(std::in_place_type<derived_impl>);
    test::expect_eq(case_name, "variant inherited explicit implementation", value.value(), result);
}
