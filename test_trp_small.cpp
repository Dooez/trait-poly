#include "shared_trait_ptr.hpp"
#include "unique_trait_ptr.hpp"

#include <print>
namespace testing {
struct trait_proto_base {
    void bar();
};

struct trait_proto : trait_proto_base {
    void foo();
};
}    // namespace testing

consteval {
    trp::define_trait<testing::trait_proto>();
}

struct some_impl {
    void foo() {
        std::println("some_impl");
    };
};
struct other_impl {
    void foo() {
        std::println("other_impl");
    };
};

int main() {
    std::println("shared");
    auto to = trp::make_shared_trait<testing::trait_proto, some_impl>();
    to.foo();
    to = trp::make_shared_trait<testing::trait_proto, other_impl>();

    std::println("unique");
    auto uptr = trp::make_unique_trait<testing::trait_proto, some_impl>();
    uptr.foo();
    uptr = trp::make_unique_trait<testing::trait_proto, other_impl>();
    uptr.foo();

    std::println("alloc_unique");
    auto auptr = trp::allocate_unique_trait<testing::trait_proto, some_impl>(std::allocator<some_impl>{});
    auptr.foo();
    auptr = trp::allocate_unique_trait<testing::trait_proto, other_impl>(std::allocator<other_impl>{});
    auptr.foo();

    return 0;
}
