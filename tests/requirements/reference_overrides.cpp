#define TRP_DEFAULT_MATCH_METHOD_ARGS
#define TRP_DEFAULT_MATCH_METHOD_REF

#include "support/requirement_support.hpp"

namespace {

struct relaxed_trait {
    [[= trp::relaxed_signature]] auto access() & -> int;
};

struct matching_cv_trait {
    [[= trp::matching_cv_signature]] auto access() const& -> int;
};

struct exact_cvref_trait {
    [[= trp::exact_cvref_signature]] auto access() const& -> int;
};

struct unqualified_impl {
    auto access() const -> int {
        return 1;
    }
};

struct lvalue_impl {
    auto access() & -> int {
        return 2;
    }
};

struct const_lvalue_impl {
    auto access() const& -> int {
        return 3;
    }
};

static_assert(!reqtest::extract_first_req<relaxed_trait>().exact_args);
static_assert(!reqtest::extract_first_req<relaxed_trait>().exact_ref);
static_assert(reqtest::extract_first_req<matching_cv_trait>().exact_args);
static_assert(reqtest::extract_first_req<matching_cv_trait>().exact_cv);
static_assert(!reqtest::extract_first_req<matching_cv_trait>().exact_ref);
static_assert(reqtest::extract_first_req<exact_cvref_trait>().exact_return);
static_assert(reqtest::extract_first_req<exact_cvref_trait>().exact_args);
static_assert(reqtest::extract_first_req<exact_cvref_trait>().exact_cv);
static_assert(reqtest::extract_first_req<exact_cvref_trait>().exact_ref);

static_assert(trp::implements_trait<unqualified_impl, relaxed_trait>);
static_assert(trp::implements_trait<unqualified_impl, matching_cv_trait>);
static_assert(!trp::implements_trait<unqualified_impl, exact_cvref_trait>);
static_assert(!trp::implements_trait<lvalue_impl, exact_cvref_trait>);
static_assert(trp::implements_trait<const_lvalue_impl, exact_cvref_trait>);

}    // namespace

int main() {}
