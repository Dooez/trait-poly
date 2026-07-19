#include "trp/dyn_trait_ref.hpp"

namespace {

inline constexpr auto reference_only_signature = trp::detail::method_signature_requirements_t{
    .exact_return = false,
    .exact_args   = false,
    .exact_cv     = false,
    .exact_ref    = true,
};

struct[[= reference_only_signature]] reference_only_trait {
    auto access() & -> int;
};

struct lvalue_impl {
    auto access() & -> int;
};

static_assert(trp::implements_trait<lvalue_impl, reference_only_trait>);

}    // namespace

int main() {
    return 0;
}
