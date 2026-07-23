#pragma once

#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace exact_arguments {

inline constexpr auto case_name       = std::string_view("requirements.exact_arguments");
inline constexpr auto template_result = 11;

struct exact_trait {
    auto value(int) -> int;
};

struct[[= trp::relaxed_signature]] relaxed_trait {
    auto value(int) -> int;
};

struct extra_default_impl {
    auto value(int input, int offset = 4) -> int {
        return input + offset;
    }
};

struct wrong_required_impl {
    auto value(short input, int offset = 4) -> int {
        return input + offset;
    }
};

struct extra_eop_default_impl {
    auto value(this extra_eop_default_impl&, int input, int offset = 0) -> int {
        return input + offset;
    }
};

struct const_template_impl {
    template<typename T>
    auto value(T) const -> int {
        return template_result;
    }
};

static_assert(trp::implements_trait<extra_default_impl, exact_trait>);
static_assert(trp::implements_trait<extra_default_impl, relaxed_trait>);
static_assert(!trp::implements_trait<wrong_required_impl, exact_trait>);
static_assert(trp::implements_trait<extra_eop_default_impl, exact_trait>);
#if defined(__clang__)
// GCC 16.1 cannot inspect the specialization needed for exact signature matching.
static_assert(trp::implements_trait<const_template_impl, exact_trait>);
#endif

inline void run() {
    auto impl        = extra_default_impl{};
    auto exact_ref   = trp::dyn_trait_ref<exact_trait>(impl);
    auto relaxed_ref = trp::dyn_trait_ref<relaxed_trait>(impl);

    test::expect_eq(case_name, "exact defaulted parameter", exact_ref.value(3), 7);
    test::expect_eq(case_name, "relaxed defaulted parameter", relaxed_ref.value(3), 7);

    auto eop_impl = extra_eop_default_impl{};
    auto eop_ref  = trp::dyn_trait_ref<exact_trait>(eop_impl);
    test::expect_eq(case_name, "exact eop defaulted parameter", eop_ref.value(5), 5);

#if defined(__clang__)
    auto template_impl = const_template_impl{};
    auto template_ref  = trp::dyn_trait_ref<exact_trait>(template_impl);
    test::expect_eq(case_name, "template cv promotion", template_ref.value(0), template_result);
#endif
}

}    // namespace exact_arguments
