#include "support/test_support.hpp"
#include "trp/trait_variant.hpp"

#include <memory>

namespace {

inline constexpr auto case_name = std::string_view("variant_alias");

struct empty_trait {};

struct nonassignable {
    static inline bool copied_from_self = false;

    int value;

    explicit nonassignable(int initial)
    : value(initial) {}

    nonassignable(nonassignable const& other) {
        copied_from_self = this == std::addressof(other);
        value            = copied_from_self ? -1 : other.value;
    }

    nonassignable(nonassignable&&)                    = delete;
    nonassignable& operator=(nonassignable const&)    = delete;
    nonassignable& operator=(nonassignable&&)         = delete;
};

}    // namespace

int main() {
    auto value = trp::trait_variant<empty_trait, nonassignable>(
        std::in_place_type<nonassignable>, 42);

    auto& alias = trp::get<nonassignable>(value);
    value       = alias;

    test::expect_eq(case_name, "source remains alive", nonassignable::copied_from_self, false);
    test::expect_eq(case_name, "value preserved", trp::get<nonassignable>(value).value, 42);
}
