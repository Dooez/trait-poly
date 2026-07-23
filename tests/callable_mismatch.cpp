#include "trp/dyn_trait_ref.hpp"

// A mismatched callable data member should make the concept false rather than
// produce a hard compile error in the function-only matcher.
struct [[= trp::exact_cvref_signature]] value_trait {
    auto value(int) & -> int;
};

struct callable {
    auto operator()(short) & -> int {
        return 1;
    }
};

struct callable_impl {
    callable value;
};

static_assert(!trp::implements_trait<callable_impl, value_trait>);

int main() {}
