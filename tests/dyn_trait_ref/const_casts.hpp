#pragma once

#include "dyn_trait_ref/support.hpp"
#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

#include <concepts>
#include <format>
#include <string_view>
#include <type_traits>

namespace const_casts {

inline constexpr auto case_name = std::string_view("dyn_trait_ref.const_casts");

template<typename T>
constexpr auto cv_name() -> std::string_view {
    if constexpr (std::is_const_v<T> and std::is_volatile_v<T>)
        return "cv";
    else if constexpr (std::is_const_v<T>)
        return "const";
    else if constexpr (std::is_volatile_v<T>)
        return "volatile";
    else
        return "unq";
}

template<typename Trait, typename Ref>
auto call_qualified_method(Ref& ref) -> int {
    if constexpr (std::is_const_v<Trait> and std::is_volatile_v<Trait>)
        return ref.cv();
    else if constexpr (std::is_const_v<Trait>)
        return ref.c();
    else if constexpr (std::is_volatile_v<Trait>)
        return ref.v();
    else
        return ref.unq();
}

template<typename Trait>
constexpr auto qualified_result() -> int {
    if constexpr (std::is_const_v<Trait> and std::is_volatile_v<Trait>)
        return cv_result;
    else if constexpr (std::is_const_v<Trait>)
        return c_result;
    else if constexpr (std::is_volatile_v<Trait>)
        return v_result;
    else
        return unq_result;
}

template<typename Target, typename Source, typename Impl>
void check_target(Impl& impl, std::string_view impl_name) {
    static_assert(trp::implements_trait<Impl, Source>);

    auto source_ref = trp::dyn_trait_ref<Source>(impl);
    auto label      = std::format("{} {} -> {}", impl_name, cv_name<Source>(), cv_name<Target>());

    constexpr auto expected = trp::implements_trait<Impl, Target>;
    test::expect_eq(case_name, label, trp::is_valid_const_trait_cast<Target>(source_ref), expected);

    if constexpr (expected) {
        auto target_ref = trp::const_trait_cast<Target>(source_ref);
        test::expect_eq(case_name,
                        std::format("{} dispatch", label),
                        call_qualified_method<Target>(target_ref),
                        qualified_result<Target>());
        test::expect_eq(
            case_name, std::format("{} runtime type", label), trp::is_holding_type<Impl>(target_ref), true);
    }
}

template<typename Source, typename Impl>
void check_source(Impl& impl, std::string_view impl_name) {
    check_target<full_trait, Source>(impl, impl_name);
    check_target<full_trait const, Source>(impl, impl_name);
    check_target<full_trait volatile, Source>(impl, impl_name);
    check_target<full_trait const volatile, Source>(impl, impl_name);
}

template<typename Impl>
void check_capability_matrix(Impl& impl, std::string_view impl_name) {
    if constexpr (trp::implements_trait<Impl, full_trait>)
        check_source<full_trait>(impl, impl_name);
    if constexpr (trp::implements_trait<Impl, full_trait const>)
        check_source<full_trait const>(impl, impl_name);
    if constexpr (trp::implements_trait<Impl, full_trait volatile>)
        check_source<full_trait volatile>(impl, impl_name);
    if constexpr (trp::implements_trait<Impl, full_trait const volatile>)
        check_source<full_trait const volatile>(impl, impl_name);
}

template<typename Expected, typename Candidate, typename Ref>
void check_runtime_type(Ref const& ref, std::string_view object_name) {
    test::expect_eq(case_name,
                    std::format("{} is {}", object_name, cv_name<Candidate>()),
                    trp::is_holding_type<Candidate>(ref),
                    std::same_as<Expected, Candidate>);
}

template<typename Expected, typename Object>
void check_runtime_types(Object& object, std::string_view object_name) {
    auto ref = trp::dyn_trait_ref<full_trait const volatile>(object);

    check_runtime_type<Expected, unq_impl>(ref, object_name);
    check_runtime_type<Expected, unq_impl const>(ref, object_name);
    check_runtime_type<Expected, unq_impl volatile>(ref, object_name);
    check_runtime_type<Expected, unq_impl const volatile>(ref, object_name);
}

inline void run() {
    auto unq = unq_impl{};
    auto c   = c_impl{};
    auto v   = v_impl{};
    auto cv  = cv_impl{};

    check_capability_matrix(unq, "unq_impl");
    check_capability_matrix(c, "c_impl");
    check_capability_matrix(v, "v_impl");
    check_capability_matrix(cv, "cv_impl");

    auto const const_unq       = unq_impl{};
    auto volatile volatile_unq = unq_impl{};
    auto const volatile cv_unq = unq_impl{};

    check_capability_matrix(const_unq, "const unq_impl");
    check_capability_matrix(volatile_unq, "volatile unq_impl");
    check_capability_matrix(cv_unq, "cv unq_impl");

    check_runtime_types<unq_impl>(unq, "unq object");
    check_runtime_types<unq_impl const>(const_unq, "const object");
    check_runtime_types<unq_impl volatile>(volatile_unq, "volatile object");
    check_runtime_types<unq_impl const volatile>(cv_unq, "cv object");
}

}    // namespace const_casts
