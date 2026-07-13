#pragma once

#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"
#include "trp/trait_variant.hpp"

namespace reqtest {

inline constexpr auto success_exit_code = 0;
inline constexpr auto short_arg         = short{3};
inline constexpr auto int_arg           = 3;

template<typename Trait>
consteval auto only_requirement() {
    constexpr auto reqs = trp::detail::all_trait_methods_and_requirements<Trait>.requirements;
    static_assert(reqs.size() == 1);
    return reqs[0];
}

}    // namespace reqtest
