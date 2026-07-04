#ifdef TRP_GODBOLT
// clang-format off
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/alias_and_helpers.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/cvtmock_trait_ref.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/trp_concepts.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/explicit_trait_impl.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/default_trait_impl.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/cvts_trait_ref.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/vtable.hpp>
                                                                                                        
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/dyn_trait_ref.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/detail/allocator_ctrl_block.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/shared_trait_ptr.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/unique_trait_ptr.hpp>
#include <https://raw.githubusercontent.com/Dooez/trait-poly/refs/heads/value/include/trp/trait_variant.hpp>
// clang-format on
#else
#include "trp/shared_trait_ptr.hpp"
#include "trp/trait_variant.hpp"
#include "trp/unique_trait_ptr.hpp"
#endif

#include <print>

enum class e0 {
};
enum class e1 {
};
enum class e2 {
};
enum class e3 {
};

struct my_trait_base {
    void foo(e0);
    void foo(e1);

    static void foo(auto& arg, e0) {
        std::println("[inline default implementation] my_trait_base::foo(e0)");
    }
};

struct my_trait : my_trait_base {
    void foo(e2);
    void foo(e3);

    void bar();
    void baz();
};
template<>
struct trp::default_impl_spec<my_trait> {
    static void baz(auto&) {
        std::println("[explicit default implementation] "
                     "trp::default_impl_spec<my_trait>::baz()");
    }
};

struct impl_base {
    void bar() {
        std::println("impl_base::bar()");
    };
};
struct some_impl : impl_base {
    // void foo(e0) { std::println("some_impl::foo(e0)"); }; missing, use
    // default
    void foo(e1) {
        std::println("some_impl::foo(e1)");
    };
    void foo(e2) {
        std::println("some_impl::foo(e2)");
    };
    void foo(e3) {
        std::println("some_impl::foo(e3)");
    };

    void baz() {
        std::println("some_impl::baz()");
    };
};
struct other_impl {
    template<typename T>
    void foo(T) {
        std::println("other_impl::foo({})", std::meta::identifier_of(^^T));
    };

    using bar_t = decltype([] { std::println("other_impl::bar_t::opeartor()()"); });
    bar_t bar{};
    // void baz() { std::println("other_impl::baz()"); }; missing, use default
};
template<>
struct trp::impl_spec_for<other_impl, my_trait> {
    // explicit implementation has higher priority
    static void foo(auto& impl, e1) {
        std::print("[explicit implementation] impl_spec_for<other_impl, "
                   "my_trait>::foo(e1); calling baz(): ");
        // call other trait methods
        impl.baz();
    }
};

#if defined(__clang__)
// required bacause of clang bug
// template class trp::dyn_trait_ref<my_trait_base>;
// template class trp::shared_trait_ptr<my_trait>;
//template class trp::trait_variant<my_trait, some_impl, other_impl>;
#endif

void test_base(trp::dyn_trait_ref<my_trait_base> ref) {
    ref.foo(e0{});
}
void test_trait(trp::shared_trait_ptr<my_trait> sh_ptr) {
    test_base(trp::trait_cast<my_trait_base>(*sh_ptr));
    sh_ptr->foo(e1{});
    sh_ptr->foo(e2{});
    sh_ptr->foo(e3{});

    sh_ptr->bar();
    sh_ptr->baz();
}

using var_t = trp::trait_variant<my_trait, some_impl, other_impl>;
void test_var_bar(var_t& v) {
    v.bar();
}

int main() {
    auto sh_ptr = trp::make_shared_trait<my_trait, some_impl>();
    test_trait(sh_ptr);
    sh_ptr = trp::make_shared_trait<my_trait, other_impl>();
    test_trait(sh_ptr);
    static_assert(sizeof(sh_ptr) == 3 * sizeof(void*));

    auto u_ptr = trp::make_unique_trait<my_trait, some_impl>();
    static_assert(sizeof(u_ptr) == 2 * sizeof(void*));

    auto au_ptr = trp::allocate_unique_trait<my_trait, some_impl>(std::allocator<some_impl>());
    static_assert(sizeof(au_ptr) == 3 * sizeof(void*));

    static_assert(trp::implements_trait<trp::dyn_trait_ref<my_trait>, my_trait>);

    std::println("Variant: ");
    auto var = var_t(some_impl{});

    var.bar();
    var = other_impl{};
    var.bar();

    return 0;
}
