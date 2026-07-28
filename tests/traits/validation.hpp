#pragma once

#include "trp/detail/trp_concepts.hpp"

namespace validation {

struct ordinary_trait {
    auto value(int) -> int;
};

struct private_data_trait {
private:
    int data;
};

struct private_base_trait : private ordinary_trait {};

struct virtual_method_trait {
    virtual auto value() -> int;
};

struct virtual_base_trait : virtual ordinary_trait {};

struct data_trait {
    int data;
};

struct static_data_trait {
    inline static int data;
};

struct special_member_trait {
    special_member_trait();
};

struct explicitly_defaulted_trait {
    explicitly_defaulted_trait() = default;

    auto value() -> int;
};

struct constexpr_static_data_trait {
    inline static constexpr int data = 0;
};

struct static_method_trait {
    static auto value() -> int;
};

struct operator_trait {
    auto operator+() -> int;
};

struct method_template_trait {
    template<typename T>
    auto value(T) -> int;
};

struct variadic_trait {
    auto value(int, ...) -> int;
};

struct eop_variadic_trait {
    auto value(this eop_variadic_trait&, int, ...) -> int;
};

struct defaulted_trait {
    auto value(int = 0) -> int;
};

struct eop_defaulted_trait {
    auto value(this eop_defaulted_trait&, int = 0) -> int;
};

struct value_eop_trait {
    auto value(this value_eop_trait) -> int;
};

struct underscore_trait {
    auto _() -> int;
};

struct unmatched_default_trait {
    auto value() -> int;

    static auto other(auto&) -> int;
};

struct inherited_invalid_trait : data_trait {};

struct left_return_trait {
    auto value() const -> int;
};

struct right_return_trait {
    auto value() const -> long;
};

struct conflicting_return_trait
: left_return_trait
, right_return_trait {};

static_assert(trp::any_trait<ordinary_trait>);
static_assert(trp::any_trait<ordinary_trait const volatile>);

// Type and inheritance restrictions.
static_assert(!trp::any_trait<int>);
static_assert(!trp::any_trait<private_data_trait>);
static_assert(!trp::any_trait<private_base_trait>);
static_assert(!trp::any_trait<virtual_method_trait>);
static_assert(!trp::any_trait<virtual_base_trait>);
static_assert(trp::detail::concepts::any_immediate_trait<inherited_invalid_trait>);
static_assert(!trp::any_trait<inherited_invalid_trait>);
static_assert(!trp::any_trait<conflicting_return_trait>);

// State and member-kind restrictions.
static_assert(!trp::any_trait<data_trait>);
static_assert(!trp::any_trait<static_data_trait>);
static_assert(!trp::any_trait<special_member_trait>);
static_assert(!trp::any_trait<explicitly_defaulted_trait>);
static_assert(!trp::any_trait<constexpr_static_data_trait>);
static_assert(!trp::any_trait<static_method_trait>);
static_assert(!trp::any_trait<operator_trait>);
static_assert(!trp::any_trait<method_template_trait>);

// Method-shape restrictions.
static_assert(!trp::any_trait<variadic_trait>);
static_assert(!trp::any_trait<eop_variadic_trait>);
static_assert(!trp::any_trait<defaulted_trait>);
static_assert(!trp::any_trait<eop_defaulted_trait>);
static_assert(!trp::any_trait<value_eop_trait>);
static_assert(!trp::any_trait<underscore_trait>);
static_assert(!trp::any_trait<unmatched_default_trait>);

}    // namespace validation
