#include "test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

#include <optional>
#include <string_view>

enum class definition_state {
    none,
    inline_only,
    specialized_only,
    inline_and_specialized,
};

enum class default_source {
    base_inline,
    base_specialized,
    derived_inline,
    derived_specialized,
};

template<>
struct std::formatter<definition_state, char> : test::enum_formatter<definition_state> {};

template<>
struct std::formatter<default_source, char> : test::enum_formatter<default_source> {};

constexpr auto has_inline(definition_state state) -> bool {
    return state == definition_state::inline_only or state == definition_state::inline_and_specialized;
}

constexpr auto has_specialized(definition_state state) -> bool {
    return state == definition_state::specialized_only or state == definition_state::inline_and_specialized;
}

constexpr auto base_default_source(definition_state base_state) -> std::optional<default_source> {
    if (has_specialized(base_state))
        return default_source::base_specialized;
    if (has_inline(base_state))
        return default_source::base_inline;
    return std::nullopt;
}

constexpr auto derived_default_source(definition_state base_state, definition_state derived_state)
    -> std::optional<default_source> {
    if (has_specialized(derived_state))
        return default_source::derived_specialized;
    if (has_inline(derived_state))
        return default_source::derived_inline;
    return base_default_source(base_state);
}

template<typename Case>
void run_case() {
    using base_trait    = typename Case::base_trait;
    using derived_trait = typename Case::derived_trait;
    using empty_impl    = typename Case::empty_impl;

    constexpr auto expected_base    = base_default_source(Case::base_state);
    constexpr auto expected_derived = derived_default_source(Case::base_state, Case::derived_state);

    static_assert(trp::explicit_supertrait_of<base_trait, derived_trait>);
    static_assert(trp::implements_trait<empty_impl, base_trait> == expected_base.has_value());
    static_assert(trp::implements_trait<empty_impl, const base_trait> == expected_base.has_value());
    static_assert(trp::implements_trait<empty_impl, derived_trait> == expected_derived.has_value());
    static_assert(trp::implements_trait<empty_impl, const derived_trait> == expected_derived.has_value());

    empty_impl impl{};

    if constexpr (trp::implements_trait<empty_impl, base_trait>) {
        auto direct_base_ref = trp::dyn_trait_ref<base_trait>(impl);
        test::expect_eq(Case::name, "direct_base.value", direct_base_ref.value(), *expected_base);
        test::expect_eq(Case::name, "direct_base.cvalue", direct_base_ref.cvalue(), *expected_base);

        auto const_base_ref = trp::dyn_trait_ref<const base_trait>(impl);
        test::expect_eq(Case::name, "const_base.cvalue", const_base_ref.cvalue(), *expected_base);
    }

    if constexpr (trp::implements_trait<empty_impl, derived_trait>) {
        auto direct_derived_ref = trp::dyn_trait_ref<derived_trait>(impl);
        test::expect_eq(Case::name, "direct_derived.value", direct_derived_ref.value(), *expected_derived);
        test::expect_eq(Case::name, "direct_derived.cvalue", direct_derived_ref.cvalue(), *expected_derived);

        auto upcast_base_ref = trp::trait_cast<base_trait>(direct_derived_ref);
        test::expect_eq(Case::name, "upcast_base.value", upcast_base_ref.value(), *expected_derived);
        test::expect_eq(Case::name, "upcast_base.cvalue", upcast_base_ref.cvalue(), *expected_derived);

        auto const_derived_ref = trp::dyn_trait_ref<const derived_trait>(impl);
        test::expect_eq(Case::name, "const_derived.cvalue", const_derived_ref.cvalue(), *expected_derived);

        auto const_upcast_base_ref = trp::trait_cast<const base_trait>(const_derived_ref);
        test::expect_eq(
            Case::name, "const_upcast_base.cvalue", const_upcast_base_ref.cvalue(), *expected_derived);
    }
}

#define TRP_BASE_INLINE_NONE
#define TRP_BASE_INLINE_INLINE                          \
    static auto value(auto&) -> default_source {        \
        return default_source::base_inline;             \
    }                                                   \
    static auto cvalue(const auto&) -> default_source { \
        return default_source::base_inline;             \
    }
#define TRP_BASE_INLINE_SPECIALIZED
#define TRP_BASE_INLINE_BOTH TRP_BASE_INLINE_INLINE

#define TRP_BASE_SPECIALIZED_NONE
#define TRP_BASE_SPECIALIZED_INLINE
#define TRP_BASE_SPECIALIZED_SPECIALIZED                \
    static auto value(auto&) -> default_source {        \
        return default_source::base_specialized;        \
    }                                                   \
    static auto cvalue(const auto&) -> default_source { \
        return default_source::base_specialized;        \
    }
#define TRP_BASE_SPECIALIZED_BOTH TRP_BASE_SPECIALIZED_SPECIALIZED

#define TRP_DERIVED_INLINE_NONE
#define TRP_DERIVED_INLINE_INLINE                       \
    static auto value(auto&) -> default_source {        \
        return default_source::derived_inline;          \
    }                                                   \
    static auto cvalue(const auto&) -> default_source { \
        return default_source::derived_inline;          \
    }
#define TRP_DERIVED_INLINE_SPECIALIZED
#define TRP_DERIVED_INLINE_BOTH TRP_DERIVED_INLINE_INLINE

#define TRP_DERIVED_SPECIALIZED_NONE
#define TRP_DERIVED_SPECIALIZED_INLINE
#define TRP_DERIVED_SPECIALIZED_SPECIALIZED             \
    static auto value(auto&) -> default_source {        \
        return default_source::derived_specialized;     \
    }                                                   \
    static auto cvalue(const auto&) -> default_source { \
        return default_source::derived_specialized;     \
    }
#define TRP_DERIVED_SPECIALIZED_BOTH TRP_DERIVED_SPECIALIZED_SPECIALIZED

#define TRP_EXPAND(x)                      x
#define TRP_BASE_INLINE_DECL(kind)         TRP_EXPAND(TRP_BASE_INLINE_##kind)
#define TRP_BASE_SPECIALIZED_DECL(kind)    TRP_EXPAND(TRP_BASE_SPECIALIZED_##kind)
#define TRP_DERIVED_INLINE_DECL(kind)      TRP_EXPAND(TRP_DERIVED_INLINE_##kind)
#define TRP_DERIVED_SPECIALIZED_DECL(kind) TRP_EXPAND(TRP_DERIVED_SPECIALIZED_##kind)

#define TRP_DEFINE_CASE(ns_name, base_kind, base_state_value, derived_kind, derived_state_value)   \
    namespace ns_name {                                                                            \
    struct base_trait {                                                                            \
        auto value() -> default_source;                                                            \
        auto cvalue() const -> default_source;                                                     \
        TRP_BASE_INLINE_DECL(base_kind)                                                            \
    };                                                                                             \
                                                                                                   \
    struct derived_trait : base_trait {                                                            \
        TRP_DERIVED_INLINE_DECL(derived_kind)                                                      \
    };                                                                                             \
                                                                                                   \
    struct empty_impl {};                                                                          \
                                                                                                   \
    struct descriptor {                                                                            \
        using base_trait                    = ns_name::base_trait;                                 \
        using derived_trait                 = ns_name::derived_trait;                              \
        using empty_impl                    = ns_name::empty_impl;                                 \
        static constexpr auto base_state    = base_state_value;                                    \
        static constexpr auto derived_state = derived_state_value;                                 \
        static constexpr auto name          = std::meta::display_string_of(^^ns_name::descriptor); \
    };                                                                                             \
    }                                                                                              \
                                                                                                   \
    template<>                                                                                     \
    struct trp::default_impl_spec<ns_name::base_trait> {                                           \
        TRP_BASE_SPECIALIZED_DECL(base_kind)                                                       \
    };                                                                                             \
                                                                                                   \
    template<>                                                                                     \
    struct trp::default_impl_spec<ns_name::derived_trait> {                                        \
        TRP_DERIVED_SPECIALIZED_DECL(derived_kind)                                                 \
    };                                                                                             \
                                                                                                   \
    consteval {                                                                                    \
        trp::define_trait<ns_name::base_trait>();                                                  \
        trp::define_trait<ns_name::derived_trait>();                                               \
    }

TRP_DEFINE_CASE(case_none_none, NONE, definition_state::none, NONE, definition_state::none)
TRP_DEFINE_CASE(case_none_inline, NONE, definition_state::none, INLINE, definition_state::inline_only)
TRP_DEFINE_CASE(
    case_none_specialized, NONE, definition_state::none, SPECIALIZED, definition_state::specialized_only)
TRP_DEFINE_CASE(case_none_both, NONE, definition_state::none, BOTH, definition_state::inline_and_specialized)

TRP_DEFINE_CASE(case_inline_none, INLINE, definition_state::inline_only, NONE, definition_state::none)
TRP_DEFINE_CASE(
    case_inline_inline, INLINE, definition_state::inline_only, INLINE, definition_state::inline_only)
TRP_DEFINE_CASE(case_inline_specialized,
                INLINE,
                definition_state::inline_only,
                SPECIALIZED,
                definition_state::specialized_only)
TRP_DEFINE_CASE(
    case_inline_both, INLINE, definition_state::inline_only, BOTH, definition_state::inline_and_specialized)

TRP_DEFINE_CASE(
    case_specialized_none, SPECIALIZED, definition_state::specialized_only, NONE, definition_state::none)
TRP_DEFINE_CASE(case_specialized_inline,
                SPECIALIZED,
                definition_state::specialized_only,
                INLINE,
                definition_state::inline_only)
TRP_DEFINE_CASE(case_specialized_specialized,
                SPECIALIZED,
                definition_state::specialized_only,
                SPECIALIZED,
                definition_state::specialized_only)
TRP_DEFINE_CASE(case_specialized_both,
                SPECIALIZED,
                definition_state::specialized_only,
                BOTH,
                definition_state::inline_and_specialized)

TRP_DEFINE_CASE(case_both_none, BOTH, definition_state::inline_and_specialized, NONE, definition_state::none)
TRP_DEFINE_CASE(
    case_both_inline, BOTH, definition_state::inline_and_specialized, INLINE, definition_state::inline_only)
TRP_DEFINE_CASE(case_both_specialized,
                BOTH,
                definition_state::inline_and_specialized,
                SPECIALIZED,
                definition_state::specialized_only)
TRP_DEFINE_CASE(case_both_both,
                BOTH,
                definition_state::inline_and_specialized,
                BOTH,
                definition_state::inline_and_specialized)

int main() {
    run_case<case_none_none::descriptor>();
    run_case<case_none_inline::descriptor>();
    run_case<case_none_specialized::descriptor>();
    run_case<case_none_both::descriptor>();

    run_case<case_inline_none::descriptor>();
    run_case<case_inline_inline::descriptor>();
    run_case<case_inline_specialized::descriptor>();
    run_case<case_inline_both::descriptor>();

    run_case<case_specialized_none::descriptor>();
    run_case<case_specialized_inline::descriptor>();
    run_case<case_specialized_specialized::descriptor>();
    run_case<case_specialized_both::descriptor>();

    run_case<case_both_none::descriptor>();
    run_case<case_both_inline::descriptor>();
    run_case<case_both_specialized::descriptor>();
    run_case<case_both_both::descriptor>();

    return 0;
}
