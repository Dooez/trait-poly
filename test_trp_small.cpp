#include "shared_trait_ptr.hpp"
#include "unique_trait_ptr.hpp"

#include <print>
namespace testing {
struct trait_proto_base {
    void bar();
};

struct trait_proto : trait_proto_base {
    void foo();
    void foo() const;
};
}    // namespace testing

consteval {
    trp::define_trait<testing::trait_proto>();
}

struct some_impl {
    void foo() {
        std::println("some_impl");
    };
    void foo() const {
        std::println("foo const some_impl");
    };
    void bar() {
        std::println("bar some_impl");
    };
    ;
};
struct other_impl_alt_base {
    void bar() {
        std::println("bar other_impl_alt_base");
    };
};
struct other_impl_base {
    void bar(void*) const {
        std::println("bar other_impl_base");
    };
};
struct other_impl
: other_impl_base
, other_impl_alt_base {
    void foo() {
        std::println("other_impl");
    };
    void foo() const {
        std::println("foo const other_impl");
    };
    // void bar() {
    //     std::println("bar other_impl");
    // };
    // void bar() {
    //     std::println("other_impl");
    // };
};
static_assert(trp::implements_trait<other_impl, testing::trait_proto>);
static_assert(
    trp::detail::implements_method<other_impl,
                                   trp::detail::trait_traits<testing::trait_proto>::all_methods[0]>());
//
int main() {
    std::println("shared");
    const auto to = trp::make_shared_trait<testing::trait_proto, some_impl>();
    to.foo();
    auto to2 = trp::make_shared_trait<testing::trait_proto, other_impl>();

    to2.bar();
    to2.foo();
    // other_impl{}.bar();

    // std::println("unique");
    // auto uptr = trp::make_unique_trait<testing::trait_proto, some_impl>();
    // // auto x = trp::detail::v2::trait_vtable_for<testing::trait_proto, some_impl>::value;
    // // constexpr auto x = trp::detail::v2::fill_vtable<testing::trait_proto_base, some_impl>();
    // uptr.foo();
    // uptr.bar();
    // uptr = trp::make_unique_trait<testing::trait_proto, other_impl>();
    // uptr.foo();
    //
    // std::println("alloc_unique");
    // auto auptr = trp::allocate_unique_trait<testing::trait_proto, some_impl>(std::allocator<some_impl>{});
    // auptr.foo();
    // auptr = trp::allocate_unique_trait<testing::trait_proto, other_impl>(std::allocator<other_impl>{});
    // auptr.foo();

    return 0;
}
