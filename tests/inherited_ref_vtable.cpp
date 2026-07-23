#include "support/test_support.hpp"
#include "trp/dyn_trait_ref.hpp"

namespace {

inline constexpr auto case_name = std::string_view("inherited_ref_vtable");

struct base_trait {
    auto access() -> int;
};

struct refined_trait : base_trait {
    auto access() & noexcept -> int;
};

struct split_impl {
    auto access() & noexcept -> int {
        return 1;
    }

    auto access() && -> int {
        return 2;
    }
};

static_assert(trp::implements_trait<split_impl, refined_trait>);

}    // namespace

int main() {
    auto impl = split_impl{};
    auto ref  = trp::dyn_trait_ref<refined_trait>(impl);
    test::expect_eq(case_name, "inherited dispatch", ref.access(), 1);
}
