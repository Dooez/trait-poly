#include "trp/dyn_trait_ref.hpp"

#include <print>

// No defaults: method stays mandatory for both base and derived traits.
// Use this when every implementation must provide behavior explicitly.
namespace no_defaults {
struct base_trait {
    void ping() const;
};

struct derived_trait : base_trait {};

struct empty_impl {};
}    // namespace no_defaults

static_assert(not trp::implements_trait<no_defaults::empty_impl, no_defaults::base_trait>);
static_assert(not trp::implements_trait<no_defaults::empty_impl, no_defaults::derived_trait>);

// Base inline default: reusable fallback shared by whole hierarchy.
namespace base_inline_inherited {
struct base_trait {
    void ping() const;

    static void ping(const auto&) {
        std::println("[base inline]");
    }
};

struct derived_trait : base_trait {};

struct empty_impl {};
}    // namespace base_inline_inherited

static_assert(trp::implements_trait<base_inline_inherited::empty_impl, base_inline_inherited::base_trait>);
static_assert(trp::implements_trait<base_inline_inherited::empty_impl, base_inline_inherited::derived_trait>);

// Base specialization: keep default body out of trait declaration.
// It also wins over same-trait inline defaults.
namespace base_specialization_wins {
struct base_trait {
    void ping() const;

    static void ping(const auto&) {
        std::println("[base inline]");
    }
};

struct derived_trait : base_trait {};

struct empty_impl {};
}    // namespace base_specialization_wins

template<>
struct trp::default_impl_spec<base_specialization_wins::base_trait> {
    static void ping(const auto&) {
        std::println("[base specialization]");
    }
};


static_assert(trp::implements_trait<base_specialization_wins::empty_impl, base_specialization_wins::base_trait>);
static_assert(trp::implements_trait<base_specialization_wins::empty_impl, base_specialization_wins::derived_trait>);

// Derived inline default: override inherited base behavior for derived-facing APIs.
// Upcasting derived ref to base keeps derived-selected behavior.
namespace derived_inline_wins {
struct base_trait {
    void ping() const;
};

struct derived_trait : base_trait {
    static void ping(const auto&) {
        std::println("[derived inline]");
    }
};

struct empty_impl {};
}    // namespace derived_inline_wins

template<>
struct trp::default_impl_spec<derived_inline_wins::base_trait> {
    static void ping(const auto&) {
        std::println("[base specialization]");
    }
};


static_assert(trp::implements_trait<derived_inline_wins::empty_impl, derived_inline_wins::base_trait>);
static_assert(trp::implements_trait<derived_inline_wins::empty_impl, derived_inline_wins::derived_trait>);

// Derived specialization: final override point when derived already has inline default.
// This is useful when declaration should stay small but derived needs custom body elsewhere.
namespace derived_specialization_wins {
struct base_trait {
    void ping() const;

    static void ping(const auto&) {
        std::println("[base inline]");
    }
};

struct derived_trait : base_trait {
    static void ping(const auto&) {
        std::println("[derived inline]");
    }
};

struct empty_impl {};
}    // namespace derived_specialization_wins

template<>
struct trp::default_impl_spec<derived_specialization_wins::derived_trait> {
    static void ping(const auto&) {
        std::println("[derived specialization]");
    }
};


static_assert(trp::implements_trait<derived_specialization_wins::empty_impl,
                                    derived_specialization_wins::base_trait>);
static_assert(trp::implements_trait<derived_specialization_wins::empty_impl,
                                    derived_specialization_wins::derived_trait>);

int main() {
    {
        using namespace base_inline_inherited;
        std::println("Case 1: base inline inherited");
        // Expected:
        //   [base inline]  direct base view
        //   [base inline]  direct derived view
        //   [base inline]  derived ref upcast to base
        empty_impl impl{};
        trp::dyn_trait_ref<const base_trait>(impl).ping();
        auto derived = trp::dyn_trait_ref<const derived_trait>(impl);
        derived.ping();
        trp::trait_cast<const base_trait>(derived).ping();
        std::println();
    }
    {
        using namespace base_specialization_wins;
        std::println("Case 2: base specialization beats base inline");
        // Expected:
        //   [base specialization]  direct base view
        //   [base specialization]  direct derived view
        //   [base specialization]  derived ref upcast to base
        empty_impl impl{};
        trp::dyn_trait_ref<const base_trait>(impl).ping();
        auto derived = trp::dyn_trait_ref<const derived_trait>(impl);
        derived.ping();
        trp::trait_cast<const base_trait>(derived).ping();
        std::println();
    }
    {
        using namespace derived_inline_wins;
        std::println("Case 3: derived inline beats inherited base default");
        // Expected:
        //   [base specialization]  direct base view
        //   [derived inline]       direct derived view
        //   [derived inline]       derived ref upcast to base
        empty_impl impl{};
        trp::dyn_trait_ref<const base_trait>(impl).ping();
        auto derived = trp::dyn_trait_ref<const derived_trait>(impl);
        derived.ping();
        trp::trait_cast<const base_trait>(derived).ping();
        std::println();
    }
    {
        using namespace derived_specialization_wins;
        std::println("Case 4: derived specialization beats derived inline");
        // Expected:
        //   [base inline]            direct base view
        //   [derived specialization] direct derived view
        //   [derived specialization] derived ref upcast to base
        empty_impl impl{};
        trp::dyn_trait_ref<const base_trait>(impl).ping();
        auto derived = trp::dyn_trait_ref<const derived_trait>(impl);
        derived.ping();
        trp::trait_cast<const base_trait>(derived).ping();
        std::println();
    }

    return 0;
}
