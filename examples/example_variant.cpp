#include "trp/trait_variant.hpp"

#include <print>


struct my_trait {
    void foo();
    void bar(int);
};

struct s0 {
    void foo() {
        std::println("s0::foo()");
    }
    void bar(int v) {
        std::println("s0::bar({})", v);
    }
};
struct s1 {
    void foo() {
        std::println("s1::foo()");
    }
    void bar(int v) {
        std::println("s1::bar({})", v);
    }
};

int main() {
    auto var0 = trp::trait_variant<my_trait, s0, s1>(s0{});
    var0.foo();

    auto var1 = trp::trait_variant<my_trait, s0, s1>(std::in_place_type<s1>);
    var1.foo();
    
    auto var2 = trp::trait_variant<my_trait, s0, s1>(std::in_place_index<0>);
    var2.foo();

    var2 = s1{};
    var2.foo();


    return 0;
}
