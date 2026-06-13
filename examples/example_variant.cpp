#include "trp/trait_variant.hpp"

#include <print>


struct my_trait {
    void foo();
    void bar(int);
    void foo() const;
};

struct s0 {
    void foo() {
        std::println("s0::foo()");
    }
    void foo() const {
        std::println("s0::foo() const");
    }
    void bar(int v) {
        std::println("s0::bar({})", v);
    }
};
struct s1 {
    s1() = default;
    s1& operator=(s1 const&) =default;
    s1& operator=(s1 &&) =default;

    s1(s1 const&){
        std::println("s1::s1(s1 const&)");

    }
    s1(s1 &&){
        std::println("s1::s1(s1&&)");

    }
    void foo() {
        std::println("s1::foo()");
    }
    void foo() const {
        std::println("s0::foo() const");
    }
    void bar(int v) {
        std::println("s1::bar({})", v);
    }
    ~s1(){
        std::println("~s1()");
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

    std::as_const(var0).foo();

    auto var3 = trp::trait_variant<my_trait const, s0, s1>(std::in_place_index<1>);
    var3.foo();

    auto var4 = var3;
    auto var5 = std::move(var3);

    return 0;
}
