#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace {

inline constexpr auto case_name = std::string_view("noexcept_conversion");
inline constexpr auto result    = 47;

struct conversion_error {};

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

struct noexcept_trait {
    auto value() noexcept -> int;
};

struct throwing_trait {
    auto value() -> int;
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
static_assert(trp::implements_trait<throwing_impl, throwing_trait>);
static_assert(trp::implements_trait<safe_impl, noexcept_trait>);
static_assert(trp::implements_trait<elision_impl, elision_trait>);

}    // namespace

int main() {
    auto safe = safe_impl{};
    auto ref  = trp::dyn_trait_ref<noexcept_trait>(safe);
    auto var  = trp::trait_variant<noexcept_trait, safe_impl>(std::in_place_type<safe_impl>);

    test::expect_eq(case_name, "dyn ref nothrow conversion", ref.value(), result);
    test::expect_eq(case_name, "variant nothrow conversion", var.value(), result);

    auto throwing     = throwing_impl{};
    auto throwing_ref = trp::dyn_trait_ref<throwing_trait>(throwing);
    try {
        (void)throwing_ref.value();
        test::fail(case_name, "throwing conversion should propagate");
    } catch (conversion_error const&) {}
}
