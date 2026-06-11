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
    auto var = trp::trait_variant<my_trait, s0, s1>(s0{});
    var.foo();

    return 0;
}
