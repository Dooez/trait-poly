#include "trp/detail/cvts_trait_ref.hpp"

#include "support/test_support.hpp"
#include "support/value_support.hpp"

#include <utility>

inline constexpr auto case_name = std::string_view("cvts_trait_ref");

using value_trait = valuetest::trait;

struct move_impl {
    int stored = 5;

    move_impl()                 = default;
    move_impl(move_impl const&) = delete;
    move_impl(move_impl&& other) noexcept
    : stored(other.stored) {
        other.stored = -1;
    }

    auto value() const -> int {
        return stored;
    }
};

struct copy_impl {
    int stored = 9;

    copy_impl()                            = default;
    copy_impl(copy_impl const&)            = default;
    copy_impl& operator=(copy_impl const&) = default;
    copy_impl(copy_impl&& other) noexcept
    : stored(other.stored) {
        other.stored = -1;
    }

    auto value() const -> int {
        return stored;
    }
};

using ref             = trp::detail::cvts_trait_ref<value_trait, move_impl>;
using const_trait_ref = trp::detail::cvts_trait_ref<value_trait const, move_impl>;
using copy_ref        = trp::detail::cvts_trait_ref<value_trait, copy_impl>;

template<typename Ref, typename To>
concept can_cast = requires(Ref&& value) { static_cast<To>(static_cast<Ref&&>(value)); };

static_assert(can_cast<ref&, move_impl&>);
static_assert(noexcept(static_cast<move_impl&>(std::declval<ref&>())));
static_assert(can_cast<ref const&, move_impl const&>);
static_assert(!can_cast<ref const&, move_impl&>);
static_assert(!can_cast<ref&, move_impl&&>);
static_assert(can_cast<ref&&, move_impl&&>);
static_assert(noexcept(static_cast<move_impl&&>(std::declval<ref&&>())));
static_assert(!can_cast<const_trait_ref&, move_impl&>);
static_assert(can_cast<const_trait_ref&, move_impl const&>);
static_assert(!can_cast<const_trait_ref&&, move_impl&&>);
static_assert(can_cast<const_trait_ref&&, move_impl const&&>);

void check_conversions() {
    auto impl    = move_impl{};
    auto adapter = ref(impl);

    auto& impl_ref = static_cast<move_impl&>(adapter);
    test::expect_eq(case_name, "lvalue reference conversion", impl_ref.value(), 5);

    auto&& impl_rref = static_cast<move_impl&&>(std::move(adapter));
    test::expect_eq(case_name, "rvalue reference conversion", &impl_rref == &impl, true);
    test::expect_eq(case_name, "reference conversion preserves source", impl.value(), 5);

    auto moved = static_cast<move_impl>(static_cast<move_impl&&>(std::move(adapter)));
    test::expect_eq(case_name, "rvalue value conversion", moved.value(), 5);
    test::expect_eq(case_name, "rvalue conversion moves source", impl.value(), -1);

    auto copy_source  = copy_impl{};
    auto copy_adapter = copy_ref(copy_source);
    auto copied       = static_cast<copy_impl>(copy_adapter);
    test::expect_eq(case_name, "lvalue value conversion", copied.value(), 9);
    test::expect_eq(case_name, "lvalue conversion copies source", copy_source.value(), 9);
}

int main() {
    check_conversions();
}
