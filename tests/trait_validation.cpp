#include "trp/detail/trp_concepts.hpp"

namespace {

struct ordinary_trait {
    auto value(int) -> int;
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

static_assert(trp::any_trait<ordinary_trait>);
static_assert(!trp::any_trait<variadic_trait>);
static_assert(!trp::any_trait<eop_variadic_trait>);
static_assert(!trp::any_trait<defaulted_trait>);
static_assert(!trp::any_trait<eop_defaulted_trait>);

}    // namespace

int main() {}
