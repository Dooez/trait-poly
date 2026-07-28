#pragma once

#include "requirements/support.hpp"

#include <functional>

namespace return_conversion {

inline constexpr auto case_name = std::string_view("requirements.return_conversion");

struct payload {
    int value;
};

struct reference_trait {
    auto get() const -> payload const&;
};

struct temporary_impl {
    auto get() const -> payload {
        return {41};
    }
};

struct proxy_impl {
    payload stored;

    auto get() const -> std::reference_wrapper<payload const> {
        return std::cref(stored);
    }
};

static_assert(!trp::implements_trait<temporary_impl, reference_trait const>);
static_assert(trp::implements_trait<proxy_impl, reference_trait const>);

inline void run() {
    auto impl = proxy_impl{.stored = {.value = 42}};
    auto ref  = trp::dyn_trait_ref<reference_trait const>(impl);

    test::expect_eq(case_name, "safe proxy return", ref.get().value, 42);
}

}    // namespace return_conversion
