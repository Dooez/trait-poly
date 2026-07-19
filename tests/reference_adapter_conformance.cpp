#define TRP_DEFAULT_MATCH_METHOD_ARGS
#define TRP_DEFAULT_MATCH_METHOD_REF

#include "trp/dyn_trait_ref.hpp"

namespace {

struct empty_impl {};

struct lvalue_default_trait {
    auto access() & -> int;

    static auto access(auto&) -> int;
};

struct rvalue_default_trait {
    auto access() && -> int;

    static auto access(auto&&) -> int;
};

struct explicit_lvalue_trait {
    auto access() & -> int;
};

struct explicit_rvalue_trait {
    auto access() && -> int;
};

struct explicit_lvalue_impl {};
struct explicit_rvalue_impl {};

}    // namespace

template<>
struct trp::impl_spec_for<explicit_lvalue_impl, explicit_lvalue_trait> {
    static auto access(auto&) -> int;
};

template<>
struct trp::impl_spec_for<explicit_rvalue_impl, explicit_rvalue_trait> {
    static auto access(auto&&) -> int;
};

static_assert(trp::implements_trait<empty_impl, lvalue_default_trait>);
static_assert(trp::implements_trait<empty_impl, rvalue_default_trait>);
static_assert(trp::implements_trait<explicit_lvalue_impl, explicit_lvalue_trait>);
static_assert(trp::implements_trait<explicit_rvalue_impl, explicit_rvalue_trait>);

int main() {
    return 0;
}
