#pragma once

#include "dyn_trait_ref/support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace cv_qualification {

inline constexpr auto case_name = std::string_view("cv_qualification");

// clang-format off
static_assert( trp::implements_trait<unq_impl, full_trait>);
static_assert( trp::implements_trait<unq_impl, full_trait const>);
static_assert( trp::implements_trait<unq_impl, full_trait volatile>);
static_assert( trp::implements_trait<unq_impl, full_trait const volatile>);
static_assert(!trp::implements_trait<c_impl,   full_trait>);
static_assert( trp::implements_trait<c_impl,   full_trait const>);
static_assert(!trp::implements_trait<c_impl,   full_trait volatile>);
static_assert( trp::implements_trait<c_impl,   full_trait const volatile>);
static_assert(!trp::implements_trait<v_impl,   full_trait>);
static_assert(!trp::implements_trait<v_impl,   full_trait const>);
static_assert( trp::implements_trait<v_impl,   full_trait volatile>);
static_assert( trp::implements_trait<v_impl,   full_trait const volatile>);
static_assert(!trp::implements_trait<cv_impl,  full_trait>);
static_assert(!trp::implements_trait<cv_impl,  full_trait const>);
static_assert(!trp::implements_trait<cv_impl,  full_trait volatile>);
static_assert( trp::implements_trait<cv_impl,  full_trait const volatile>);

// dyn_trait_ref is not cv transitive
static_assert(can_unq<ref_unq&>);
static_assert(  can_c<ref_unq&>);
static_assert(  can_v<ref_unq&>);
static_assert( can_cv<ref_unq&>);
static_assert(can_unq<ref_unq const&>);
static_assert(  can_c<ref_unq const&>);
static_assert(  can_v<ref_unq const&>);
static_assert( can_cv<ref_unq const&>);
static_assert(can_unq<ref_unq volatile&>);
static_assert(  can_c<ref_unq volatile&>);
static_assert(  can_v<ref_unq volatile&>);
static_assert( can_cv<ref_unq volatile&>);
static_assert(can_unq<ref_unq const volatile&>);
static_assert(  can_c<ref_unq const volatile&>);
static_assert(  can_v<ref_unq const volatile&>);
static_assert( can_cv<ref_unq const volatile&>);

static_assert(!can_unq<ref_c&>);
static_assert(   can_c<ref_c&>);
static_assert(  !can_v<ref_c&>);
static_assert(  can_cv<ref_c&>);

static_assert(!can_unq<ref_v&>);
static_assert(  !can_c<ref_v&>);
static_assert(   can_v<ref_v&>);
static_assert(  can_cv<ref_v&>);

static_assert(!can_unq<ref_cv&>);
static_assert(  !can_c<ref_cv&>);
static_assert(  !can_v<ref_cv&>);
static_assert(  can_cv<ref_cv&>);

// var is cv-transitive
static_assert( can_unq<var_unq&>);
static_assert(   can_c<var_unq&>);
static_assert(   can_v<var_unq&>);
static_assert(  can_cv<var_unq&>);
static_assert(!can_unq<var_unq const&>);
static_assert(   can_c<var_unq const&>);
static_assert(  !can_v<var_unq const&>);
static_assert(  can_cv<var_unq const&>);
static_assert(!can_unq<var_unq volatile&>);
static_assert(  !can_c<var_unq volatile&>);
static_assert(   can_v<var_unq volatile&>);
static_assert(  can_cv<var_unq volatile&>);
static_assert(!can_unq<var_unq const volatile&>);
static_assert(  !can_c<var_unq const volatile&>);
static_assert(  !can_v<var_unq const volatile&>);
static_assert(  can_cv<var_unq const volatile&>);

static_assert(!can_unq<var_c&>);
static_assert(   can_c<var_c&>);
static_assert(  !can_v<var_c&>);
static_assert(  can_cv<var_c&>);

static_assert(!can_unq<var_v&>);
static_assert(  !can_c<var_v&>);
static_assert(   can_v<var_v&>);
static_assert(  can_cv<var_v&>);

static_assert(!can_unq<var_cv&>);
static_assert(  !can_c<var_cv&>);
static_assert(  !can_v<var_cv&>);
static_assert(  can_cv<var_cv&>);

// clang-format on

void call_cv_ref_methods() {
    auto impl = unq_impl{};

    auto unq_ref = ref_unq(impl);
    test::expect_eq(case_name, "unq ref calls unq method", unq_ref.unq(), unq_result);
    test::expect_eq(case_name, "unq ref calls c method", unq_ref.c(), c_result);
    test::expect_eq(case_name, "unq ref calls v method", unq_ref.v(), v_result);
    test::expect_eq(case_name, "unq ref calls cv method", unq_ref.cv(), cv_result);

    auto const_ref = ref_c(impl);
    test::expect_eq(case_name, "const ref calls c method", const_ref.c(), c_result);
    test::expect_eq(case_name, "const ref calls cv method", const_ref.cv(), cv_result);

    auto volatile_ref = ref_v(impl);
    test::expect_eq(case_name, "volatile ref calls v method", volatile_ref.v(), v_result);
    test::expect_eq(case_name, "volatile ref calls cv method", volatile_ref.cv(), cv_result);

    auto cv_ref = ref_cv(impl);
    test::expect_eq(case_name, "cv ref calls cv method", cv_ref.cv(), cv_result);
}

inline void run() {
    call_cv_ref_methods();
}

}    // namespace cv_qualification
