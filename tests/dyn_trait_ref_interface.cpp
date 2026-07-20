#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

#include <type_traits>

namespace {

inline constexpr auto case_name = std::string_view("dyn_trait_ref_interface");

struct split_trait {
    auto value() & -> int;
    auto value() && -> int;
};

struct split_impl {
    auto value() & -> int {
        return 1;
    }

    auto value() && -> int {
        return 2;
    }
};

struct argument_trait {
    auto value(int) noexcept -> int;
};

struct argument_impl {
    auto value(int input) noexcept -> int {
        return input;
    }
};

using argument_ref = trp::dyn_trait_ref<argument_trait>;

template<typename Ref, typename Arg>
concept can_call_value = requires(Ref& ref, Arg&& arg) { ref.value(static_cast<Arg&&>(arg)); };

static_assert(can_call_value<argument_ref, int>);
static_assert(!can_call_value<argument_ref, char const*>);
static_assert(noexcept(std::declval<argument_ref&>().value(1)));

}    // namespace

int main() {
    auto impl = split_impl{};
    auto ref  = trp::dyn_trait_ref<split_trait>(impl);

    test::expect_eq(case_name, "split trait constructs and calls lvalue entry", ref.value(), 1);
}
