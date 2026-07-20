// Exact reference matching is only valid together with exact argument matching.
#define TRP_DEFAULT_MATCH_METHOD_ARGS
#define TRP_DEFAULT_MATCH_METHOD_REF

#include "support/ref_support.hpp"
#include "support/requirement_support.hpp"

namespace {

using namespace reftest;

static_assert(reqtest::extract_first_req<unqualified_trait>().exact_ref);
static_assert(reqtest::extract_first_req<lvalue_trait>().exact_ref);
static_assert(reqtest::extract_first_req<rvalue_trait>().exact_ref);
static_assert(reqtest::extract_first_req<unqualified_trait>().exact_args);
static_assert(reqtest::extract_first_req<lvalue_trait>().exact_args);
static_assert(reqtest::extract_first_req<rvalue_trait>().exact_args);

// clang-format off
static_assert(    trp::implements_trait<unqualified_impl,  unqualified_trait>);
static_assert(not trp::implements_trait<lvalue_impl,       unqualified_trait>);
static_assert(not trp::implements_trait<rvalue_impl,       unqualified_trait>);

static_assert(not trp::implements_trait<unqualified_impl,  lvalue_trait>);
static_assert(    trp::implements_trait<lvalue_impl,       lvalue_trait>);
static_assert(not trp::implements_trait<rvalue_impl,       lvalue_trait>);

static_assert(not trp::implements_trait<unqualified_impl,  rvalue_trait>);
static_assert(not trp::implements_trait<lvalue_impl,       rvalue_trait>);
static_assert(    trp::implements_trait<rvalue_impl,       rvalue_trait>);

static_assert(not trp::implements_trait<unqualified_impl,  split_trait>);
static_assert(not trp::implements_trait<lvalue_impl,       split_trait>);
static_assert(not trp::implements_trait<rvalue_impl,       split_trait>);
static_assert(    trp::implements_trait<split_impl,        split_trait>);
static_assert(    trp::implements_trait<eop_split_impl,    split_trait>);
#if defined(__clang__)
// GCC 16.1 cannot inspect the specialization needed for exact signature matching.
static_assert(    trp::implements_trait<forwarding_impl, split_trait>);
#endif
static_assert(    trp::implements_trait<callable_impl,    split_trait>);
// clang-format on

}    // namespace

int main() {
    return 0;
}
