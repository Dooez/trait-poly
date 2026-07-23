#pragma once

#include "support/ref_support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace inherited_ref_vtable {

inline constexpr auto case_name = std::string_view("inherited_ref_vtable");

using base_trait = reftest::unqualified_trait;

struct refined_trait : base_trait {
    auto access() & noexcept -> int;
};

static_assert(trp::implements_trait<reftest::split_impl, refined_trait>);

inline void run() {
    auto impl = reftest::split_impl{};
    auto ref  = trp::dyn_trait_ref<refined_trait>(impl);
    test::expect_eq(case_name, "inherited dispatch", ref.access(), reftest::lvalue_result);
}

}    // namespace inherited_ref_vtable
