#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace explicit_noexcept {

inline constexpr auto case_name       = std::string_view("explicit_noexcept");
inline constexpr auto inline_result   = 2;
inline constexpr auto explicit_result = 3;

struct inline_trait {
    auto value() -> int;

    static auto value(auto&) noexcept -> int {
        return inline_result;
    }
};

struct explicit_trait {
    auto value() -> int;
};

struct explicit_impl {};

struct required_noexcept_trait {
    auto value() noexcept -> int;
};

struct throwing_explicit_impl {};

struct empty_impl {};

}    // namespace explicit_noexcept

template<>
struct trp::impl_spec_for<explicit_noexcept::explicit_impl, explicit_noexcept::explicit_trait> {
    static auto value(auto&) noexcept -> int {
        return explicit_noexcept::explicit_result;
    }
};

template<>
struct trp::impl_spec_for<explicit_noexcept::throwing_explicit_impl,
                          explicit_noexcept::required_noexcept_trait> {
    static auto value(auto&) -> int {
        return 0;
    }
};

namespace explicit_noexcept {

static_assert(trp::any_trait<inline_trait>);
static_assert(trp::implements_trait<empty_impl, inline_trait>);
static_assert(trp::implements_trait<explicit_impl, explicit_trait>);
static_assert(!trp::implements_trait<throwing_explicit_impl, required_noexcept_trait>);

}    // namespace explicit_noexcept

int main() {
    using namespace explicit_noexcept;

    auto empty = empty_impl{};
    auto ref   = trp::dyn_trait_ref<inline_trait>(empty);
    test::expect_eq(case_name, "inline noexcept promotion", ref.value(), inline_result);

    auto value = trp::trait_variant<explicit_trait, explicit_impl>(std::in_place_type<explicit_impl>);
    test::expect_eq(case_name, "explicit noexcept promotion", value.value(), explicit_result);
}
