#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace {

inline constexpr auto case_name = std::string_view("static_template");

struct [[= trp::exact_signature]] value_trait {
    auto value(int) -> int;
};

struct static_template_impl {
    template<typename T>
    static auto value(T input) -> int {
        return input + 1;
    }
};

static_assert(trp::implements_trait<static_template_impl, value_trait>,
              "exact static function-template specialization should be accepted");

}    // namespace

int main() {
    auto impl = static_template_impl{};
    auto ref  = trp::dyn_trait_ref<value_trait>(impl);
    test::expect_eq(case_name, "static template dispatch", ref.value(1), 2);
}
