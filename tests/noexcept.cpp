#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

template<int>
struct empty_type {};

struct value_trait {
    auto value() -> int;
};

struct noexcept_trait {
    auto value() noexcept -> int;
};

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

using empty_impl = empty_type<0>;

}    // namespace explicit_noexcept

template<>
struct trp::impl_spec_for<explicit_noexcept::empty_impl, value_trait> {
    static auto value(auto&) noexcept -> int {
        return explicit_noexcept::explicit_result;
    }
};

template<>
struct trp::impl_spec_for<explicit_noexcept::empty_impl, noexcept_trait> {
    static auto value(auto&) -> int {
        return 0;
    }
};

namespace explicit_noexcept {

static_assert(trp::any_trait<inline_trait>);
static_assert(trp::implements_trait<empty_impl, inline_trait>);
static_assert(trp::implements_trait<empty_impl, value_trait>);
static_assert(!trp::implements_trait<empty_impl, noexcept_trait>);

}    // namespace explicit_noexcept

void check_explicit_implementations() {
    using namespace explicit_noexcept;

    auto empty = empty_impl{};
    auto ref   = trp::dyn_trait_ref<inline_trait>(empty);
    test::expect_eq(case_name, "inline noexcept promotion", ref.value(), inline_result);

    auto value = trp::trait_variant<value_trait, empty_impl>(std::in_place_type<empty_impl>);
    test::expect_eq(case_name, "explicit noexcept promotion", value.value(), explicit_result);
}

namespace noexcept_conversion {

inline constexpr auto case_name = std::string_view("noexcept_conversion");
inline constexpr auto result    = 47;
using conversion_error          = empty_type<1>;

struct throwing_result {
    operator int() const {
        throw conversion_error{};
    }
};

struct safe_result {
    operator int() const noexcept {
        return result;
    }
};

struct throwing_impl {
    auto value() noexcept -> throwing_result {
        return {};
    }
};

struct safe_impl {
    auto value() noexcept -> safe_result {
        return {};
    }
};

struct immovable_result {
    immovable_result()                        = default;
    immovable_result(immovable_result const&) = delete;
    immovable_result(immovable_result&&)      = delete;
};

struct elision_trait {
    auto value() noexcept -> immovable_result;
};

struct elision_impl {
    auto value() noexcept -> immovable_result {
        return {};
    }
};

static_assert(!trp::implements_trait<throwing_impl, noexcept_trait>);
static_assert(trp::implements_trait<throwing_impl, value_trait>);
static_assert(trp::implements_trait<safe_impl, noexcept_trait>);
static_assert(trp::implements_trait<elision_impl, elision_trait>);

void check_conversions() {
    auto safe = safe_impl{};
    auto ref  = trp::dyn_trait_ref<noexcept_trait>(safe);
    auto var  = trp::trait_variant<noexcept_trait, safe_impl>(std::in_place_type<safe_impl>);

    test::expect_eq(case_name, "dyn ref nothrow conversion", ref.value(), result);
    test::expect_eq(case_name, "variant nothrow conversion", var.value(), result);

    auto throwing     = throwing_impl{};
    auto throwing_ref = trp::dyn_trait_ref<value_trait>(throwing);
    try {
        (void)throwing_ref.value();
        test::fail(case_name, "throwing conversion should propagate");
    } catch (conversion_error const&) {}
}

}    // namespace noexcept_conversion

int main() {
    check_explicit_implementations();
    noexcept_conversion::check_conversions();
}
