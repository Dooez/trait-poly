#pragma once

#include "requirements/support.hpp"
#include "support/ref_support.hpp"

#include <utility>

namespace reference_qualification {

using namespace reftest;

static_assert(extract_first_req<unqualified_trait>().exact_ref);
static_assert(extract_first_req<lvalue_trait>().exact_ref);
static_assert(extract_first_req<rvalue_trait>().exact_ref);
static_assert(extract_first_req<unqualified_trait>().exact_args);
static_assert(extract_first_req<lvalue_trait>().exact_args);
static_assert(extract_first_req<rvalue_trait>().exact_args);

// clang-format off
static_assert(    trp::implements_trait<unqualified_impl,  unqualified_trait>);
static_assert(not trp::implements_trait<lvalue_impl,       unqualified_trait>);
static_assert(not trp::implements_trait<rvalue_impl,       unqualified_trait>);
static_assert(not trp::implements_trait<callable_impl,     unqualified_trait>);

static_assert(not trp::implements_trait<unqualified_impl,  lvalue_trait>);
static_assert(    trp::implements_trait<lvalue_impl,       lvalue_trait>);
static_assert(not trp::implements_trait<rvalue_impl,       lvalue_trait>);
static_assert(    trp::implements_trait<callable_impl,     lvalue_trait>);

static_assert(not trp::implements_trait<unqualified_impl,  rvalue_trait>);
static_assert(not trp::implements_trait<lvalue_impl,       rvalue_trait>);
static_assert(    trp::implements_trait<rvalue_impl,       rvalue_trait>);
static_assert(    trp::implements_trait<callable_impl,     rvalue_trait>);

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
static_assert(    trp::implements_trait<cvref_impl,       cvref_trait>);
// clang-format on

template<typename T>
concept can_access = requires(T&& value) { std::forward<T>(value).access(); };

using lvalue_ref = trp::dyn_trait_ref<lvalue_trait>;
using rvalue_ref = trp::dyn_trait_ref<rvalue_trait>;
using split_ref  = trp::dyn_trait_ref<split_trait>;

// A dyn_trait_ref is a non-owning view: it omits rvalue-only methods and does
// not make the remaining lvalue-qualified methods ref-transitive.
static_assert(can_access<lvalue_ref&>);
static_assert(can_access<lvalue_ref>);
static_assert(!can_access<rvalue_ref&>);
static_assert(!can_access<rvalue_ref>);
static_assert(can_access<split_ref&>);
static_assert(can_access<split_ref>);

}    // namespace reference_qualification
