#include "shared_trait.hpp"
#include "testing.hpp"

#include <print>

// clang-format off
struct trait_proto {
    void baz(e2);

    void bar00(e0){std::println("bar00(e0)");};
    void bar01(e0){std::println("bar01(e0)");};
    void bar02(e0){std::println("bar02(e0)");};
    void bar03(e0){std::println("bar03(e0)");};
    void bar04(e0){std::println("bar04(e0)");};
    void bar05(e0){std::println("bar05(e0)");};
    void bar06(e0){std::println("bar06(e0)");};
    void bar07(e0){std::println("bar07(e0)");};
    void bar08(e0){std::println("bar08(e0)");};
    void bar09(e0){std::println("bar09(e0)");};
    void bar10(e0){std::println("bar10(e0)");};
    void bar11(e0){std::println("bar11(e0)");};
    void bar12(e0){std::println("bar12(e0)");};
    void bar13(e0){std::println("bar13(e0)");};
    void bar14(e0){std::println("bar14(e0)");};
    void bar15(e0){std::println("bar15(e0)");};
    void bar16(e0){std::println("bar16(e0)");};
    void bar17(e0){std::println("bar17(e0)");};
    void bar18(e0){std::println("bar18(e0)");};
    void bar19(e0){std::println("bar19(e0)");};
    void bar20(e0){std::println("bar20(e0)");};
    void bar21(e0){std::println("bar21(e0)");};
    void bar22(e0){std::println("bar22(e0)");};
    void bar23(e0){std::println("bar23(e0)");};
    void bar24(e0){std::println("bar24(e0)");};
    void bar25(e0){std::println("bar25(e0)");};
    void bar26(e0){std::println("bar26(e0)");};
    void bar27(e0){std::println("bar27(e0)");};
    void bar28(e0){std::println("bar28(e0)");};
    void bar29(e0){std::println("bar29(e0)");};
    void bar30(e0){std::println("bar30(e0)");};
    void bar31(e0){std::println("bar31(e0)");};
    void bar32(e0){std::println("bar32(e0)");};

    void bar(e00){std::println("bar(e00)");};
    void bar(e01){std::println("bar(e01)");};
    void bar(e02){std::println("bar(e02)");};
    void bar(e03){std::println("bar(e03)");};
    void bar(e04){std::println("bar(e04)");};
    void bar(e05){std::println("bar(e05)");};
    void bar(e06){std::println("bar(e06)");};
    void bar(e07){std::println("bar(e07)");};
    void bar(e08){std::println("bar(e08)");};
    void bar(e09){std::println("bar(e09)");};
    void bar(e10){std::println("bar(e10)");};
    void bar(e11){std::println("bar(e11)");};
    void bar(e12){std::println("bar(e12)");};
    void bar(e13){std::println("bar(e13)");};
    void bar(e14){std::println("bar(e14)");};
    void bar(e15){std::println("bar(e15)");};
    void bar(e16){std::println("bar(e16)");};
    void bar(e17){std::println("bar(e17)");};
    void bar(e18){std::println("bar(e18)");};
    void bar(e19){std::println("bar(e19)");};
    void bar(e20){std::println("bar(e20)");};
    void bar(e21){std::println("bar(e21)");};
    void bar(e22){std::println("bar(e22)");};
    void bar(e23){std::println("bar(e23)");};
    void bar(e24){std::println("bar(e24)");};
    void bar(e25){std::println("bar(e25)");};
    void bar(e26){std::println("bar(e26)");};
    void bar(e27){std::println("bar(e27)");};
    void bar(e28){std::println("bar(e28)");};
    void bar(e29){std::println("bar(e29)");};
    void bar(e30){std::println("bar(e30)");};
    void bar(e31){std::println("bar(e31)");};
    void bar(e32){std::println("bar(e32)");};

};

consteval {
    trp::define_trait<trait_proto>();
}

// clang-format on

struct my_trait {
    void foo();
};
struct not_trait_data {
    int  v;
    void foo();
};
struct not_trait_stdata {
    static int v;
    void       foo();
};
struct not_trait_empty_fn {};

struct not_trait_virt_fn {
    virtual int foo();
    void        bar();
};
struct not_trait_template {
    template<typename X>
    void foo(X);
};

using namespace trp;
static_assert(any_trait<my_trait>);
static_assert(not any_trait<not_trait_data>);
static_assert(not any_trait<not_trait_stdata>);
static_assert(not any_trait<not_trait_empty_fn>);
static_assert(not any_trait<not_trait_virt_fn>);
static_assert(not any_trait<not_trait_template>);

int main() {
    auto to = make_shared_trait<trait_proto, some_trait_impl>();
    std::println("sizeof shared trait object {}", sizeof(to));
    std::println("sizeof trait impl {}", sizeof(detail::trait_impl<trait_proto>));
    auto toptr = &to;

    test_16bars(to);
    to = allocate_shared_trait<trait_proto, other_trait_impl>(std::allocator<std::byte>{});
    test_16bars(to);
}
