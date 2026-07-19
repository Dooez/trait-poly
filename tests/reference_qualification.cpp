#include "support/ref_support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

#include <utility>

namespace {

using namespace reftest;

inline constexpr auto case_name = std::string_view("reference_qualification");

// clang-format off
static_assert( trp::implements_trait<unqualified_impl, unqualified_trait>);
static_assert( trp::implements_trait<lvalue_impl,       unqualified_trait>);
static_assert(!trp::implements_trait<rvalue_impl,       unqualified_trait>);

static_assert( trp::implements_trait<unqualified_impl, lvalue_trait>);
static_assert( trp::implements_trait<lvalue_impl,       lvalue_trait>);
static_assert(!trp::implements_trait<rvalue_impl,       lvalue_trait>);

static_assert( trp::implements_trait<unqualified_impl, rvalue_trait>);
static_assert(!trp::implements_trait<lvalue_impl,       rvalue_trait>);
static_assert( trp::implements_trait<rvalue_impl,       rvalue_trait>);

static_assert( trp::implements_trait<unqualified_impl, split_trait>);
static_assert(!trp::implements_trait<lvalue_impl,       split_trait>);
static_assert(!trp::implements_trait<rvalue_impl,       split_trait>);
static_assert( trp::implements_trait<split_impl,        split_trait>);
static_assert( trp::implements_trait<eop_split_impl,    split_trait>);
static_assert( trp::implements_trait<forwarding_impl,   split_trait>);
static_assert( trp::implements_trait<callable_impl,     split_trait>);
static_assert( trp::implements_trait<cvref_impl,        cvref_trait>);
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

template<typename Impl>
void check_split_dispatch(std::string_view lvalue_check, std::string_view rvalue_check) {
    using variant = trp::trait_variant<split_trait, Impl>;

    auto value = variant(std::in_place_type<Impl>);
    test::expect_eq(case_name, lvalue_check, value.access(), lvalue_result);
    test::expect_eq(case_name, rvalue_check, std::move(value).access(), rvalue_result);
}

void check_unqualified_dispatch() {
    using variant = trp::trait_variant<split_trait, unqualified_impl>;

    auto value = variant(std::in_place_type<unqualified_impl>);
    test::expect_eq(case_name, "unqualified implementation as lvalue", value.access(), unqualified_result);
    test::expect_eq(
        case_name, "unqualified implementation as rvalue", std::move(value).access(), unqualified_result);
}

void check_cvref_dispatch() {
    using variant = trp::trait_variant<cvref_trait, cvref_impl>;

    auto value = variant(std::in_place_type<cvref_impl>);
    test::expect_eq(case_name, "mutable lvalue", value.access(), lvalue_result);
    test::expect_eq(case_name, "mutable rvalue", std::move(value).access(), rvalue_result);

    auto const_value = variant(std::in_place_type<cvref_impl>);
    test::expect_eq(case_name, "const lvalue", std::as_const(const_value).access(), const_lvalue_result);
    test::expect_eq(
        case_name, "const rvalue", std::move(std::as_const(const_value)).access(), const_rvalue_result);

    auto                 volatile_value = variant(std::in_place_type<cvref_impl>);
    auto volatile&       volatile_ref   = volatile_value;
    auto const volatile& cv_ref         = volatile_value;
    test::expect_eq(case_name, "volatile lvalue", volatile_ref.access(), volatile_lvalue_result);
    test::expect_eq(case_name, "volatile rvalue", std::move(volatile_ref).access(), volatile_rvalue_result);
    test::expect_eq(case_name, "cv lvalue", cv_ref.access(), cv_lvalue_result);
    test::expect_eq(case_name, "cv rvalue", std::move(cv_ref).access(), cv_rvalue_result);
}

}    // namespace

int main() {
    // check_unqualified_dispatch();
    // check_split_dispatch<split_impl>("ordinary split lvalue", "ordinary split rvalue");
    // check_split_dispatch<eop_split_impl>("explicit-object split lvalue", "explicit-object split rvalue");
#if 1
    // TODO: clang-p2996 rejects splicing this deducing-this member template with
    // "cannot implicitly reference a class member through a splice".
    check_split_dispatch<forwarding_impl>("forwarding explicit-object lvalue",
                                          "forwarding explicit-object rvalue");
#endif
    check_split_dispatch<callable_impl>("callable data member lvalue", "callable data member rvalue");
    check_cvref_dispatch();

    return 0;
}
