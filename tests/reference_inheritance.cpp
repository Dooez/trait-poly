#include "support/test_support.hpp"
#include "trp/trait_variant.hpp"

#include <string_view>
#include <utility>

namespace {

inline constexpr auto case_name     = std::string_view("reference_inheritance");
inline constexpr auto lvalue_result = 101;
inline constexpr auto rvalue_result = 202;

struct split_impl {
    auto access() & noexcept -> int {
        return lvalue_result;
    }

    auto access() && -> int {
        return rvalue_result;
    }
};

struct throwing_lvalue_impl {
    auto access() & -> int {
        return lvalue_result;
    }

    auto access() && -> int {
        return rvalue_result;
    }
};

namespace unqualified_then_lvalue {

struct base_trait {
    auto access() -> int;
};

struct trait : base_trait {
    auto access() & noexcept -> int;
};

static_assert(trp::implements_trait<split_impl, trait>);
static_assert(!trp::implements_trait<throwing_lvalue_impl, trait>);

}    // namespace unqualified_then_lvalue

namespace unqualified_then_rvalue {

struct base_trait {
    auto access() -> int;
};

struct trait : base_trait {
    auto access() && -> int;
};

static_assert(trp::implements_trait<split_impl, trait>);

}    // namespace unqualified_then_rvalue

namespace split_bases {

struct lvalue_trait {
    auto access() & noexcept -> int;
};

struct rvalue_trait {
    auto access() && -> int;
};

struct trait
: lvalue_trait
, rvalue_trait {};

static_assert(trp::implements_trait<split_impl, trait>);
static_assert(!trp::implements_trait<throwing_lvalue_impl, trait>);

}    // namespace split_bases

namespace duplicate_lvalue_bases {

struct first_trait {
    auto access() & noexcept -> int;
};

struct second_trait {
    auto access() & noexcept -> int;
};

struct trait
: first_trait
, second_trait {};

static_assert(trp::implements_trait<split_impl, trait>);

}    // namespace duplicate_lvalue_bases

template<typename Trait>
void check_split_dispatch(std::string_view lvalue_check, std::string_view rvalue_check) {
    using variant = trp::trait_variant<Trait, split_impl>;

    auto value = variant(std::in_place_type<split_impl>);
    test::expect_eq(case_name, lvalue_check, value.access(), lvalue_result);
    test::expect_eq(case_name, rvalue_check, std::move(value).access(), rvalue_result);
}

void check_duplicate_dispatch() {
    using variant = trp::trait_variant<duplicate_lvalue_bases::trait, split_impl>;

    auto value = variant(std::in_place_type<split_impl>);
    test::expect_eq(case_name, "duplicate lvalue methods deduplicated", value.access(), lvalue_result);
}

}    // namespace

int main() {
    check_split_dispatch<unqualified_then_lvalue::trait>("unqualified base plus lvalue redeclaration",
                                                         "unqualified base supplies rvalue");
    check_split_dispatch<unqualified_then_rvalue::trait>("unqualified base supplies lvalue",
                                                         "unqualified base plus rvalue redeclaration");
    check_split_dispatch<split_bases::trait>("lvalue method from first base",
                                             "rvalue method from second base");
    check_duplicate_dispatch();

    return 0;
}
