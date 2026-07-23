#pragma once

#include "support/test_support.hpp"
#include "trait_ptr/support.hpp"
#include "trp/shared_trait_ptr.hpp"
#include "trp/unique_trait_ptr.hpp"

#include <cstddef>
#include <memory>
#include <utility>

namespace access {

inline constexpr auto initial_value = 21;
inline constexpr auto updated_value = 22;
inline constexpr auto case_name     = std::string_view("trait_ptr.access");

struct other_node {
    int stored = 0;

    auto value() const -> int {
        return stored;
    }

    auto set(int value) -> void {
        stored = value;
    }
};

template<typename Ptr>
void check_accessors(Ptr& ptr) {
    test::expect_eq(case_name, "type-erased get", ptr.get() != nullptr, true);
    test::expect_eq(case_name, "typed get", ptr.template get<node>() != nullptr, true);
    test::expect_eq(case_name,
                    "typed and erased get agree",
                    ptr.get() == static_cast<void*>(ptr.template get<node>()),
                    true);
    test::expect_eq(case_name, "mismatched get", ptr.template get<other_node>() == nullptr, true);
    test::expect_eq(case_name, "holding node", trp::is_holding_type<node>(ptr), true);
    test::expect_eq(case_name, "not holding other node", trp::is_holding_type<other_node>(ptr), false);

    (*ptr).set(updated_value);
    test::expect_eq(case_name, "operator* dispatch", (*ptr).value(), updated_value);
}

template<typename Ptr>
void check_empty_accessors(Ptr const& ptr, std::string_view label) {
    test::expect_eq(case_name, label, static_cast<bool>(ptr), false);
    test::expect_eq(case_name, "empty type-erased get", ptr.get() == nullptr, true);
    test::expect_eq(case_name, "empty typed get", ptr.template get<node>() == nullptr, true);
    test::expect_eq(case_name, "empty holding type", trp::is_holding_type<node>(ptr), false);
}

void check_public_accessors() {
    auto state = counts{};
    {
        auto unique = trp::make_unique_trait<write_trait, node>(state, initial_value);
        check_accessors(unique);

        auto allocated =
            trp::allocate_unique_trait<write_trait, node>(std::allocator<std::byte>{}, state, initial_value);
        check_accessors(allocated);

        auto shared = trp::make_shared_trait<write_trait, node>(state, initial_value);
        check_accessors(shared);
    }

    test::expect_eq(case_name, "access objects destroyed", state.destroyed, 3);
}

void check_empty_handles() {
    auto unique = trp::unique_trait_ptr<write_trait>{};
    check_empty_accessors(unique, "default unique empty");
    auto unique_base = trp::trait_cast<read_trait>(std::move(unique));
    check_empty_accessors(unique_base, "empty unique upcast");

    auto allocated = trp::alloc_unique_trait_ptr<write_trait>{};
    check_empty_accessors(allocated, "default allocated unique empty");
    auto allocated_base = trp::trait_cast<read_trait>(std::move(allocated));
    check_empty_accessors(allocated_base, "empty allocated unique upcast");

    auto shared = trp::shared_trait_ptr<write_trait>{};
    check_empty_accessors(shared, "default shared empty");
    auto shared_base = trp::trait_cast<read_trait>(shared);
    check_empty_accessors(shared_base, "empty shared upcast");
}

inline void run() {
    check_public_accessors();
    check_empty_handles();
}

}    // namespace access
