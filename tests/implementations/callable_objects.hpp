#pragma once

#include "support/ref_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace callable_objects {

struct[[= trp::exact_cvref_signature]] value_trait {
    auto value(int) & -> int;
};

struct mismatch {
    auto operator()(short) & -> int {
        return 1;
    }
};

struct mismatch_impl {
    mismatch value;
};

// Ref-qualified call operators can implement the corresponding trait methods.
static_assert(trp::implements_trait<reftest::callable_impl, reftest::split_trait>);

// A mismatched callable signature is rejected without a hard matcher error.
static_assert(!trp::implements_trait<mismatch_impl, value_trait>);

}    // namespace callable_objects
