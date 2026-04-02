#include "shared_trait_ptr.hpp"
#include "unique_trait_ptr.hpp"

#include <print>
namespace testing {
struct trait_proto_base {
    void bar();
};
consteval {
    trp::define_trait<trait_proto_base>();
}

struct trait_proto : trait_proto_base {
    void foo();
    void foo() const;
    void foo() const volatile;
};
}    // namespace testing

consteval {
    trp::define_trait<testing::trait_proto>();
}

static_assert(trp::supertrait_of<testing::trait_proto_base, testing::trait_proto>);
static_assert(trp::supertrait_of<testing::trait_proto, testing::trait_proto>);
static_assert(not trp::direct_supertrait_of<testing::trait_proto, testing::trait_proto>);
static_assert(trp::explicit_supertrait_of<testing::trait_proto, testing::trait_proto>);
static_assert(not trp::supertrait_of<testing::trait_proto, testing::trait_proto_base>);

// NOLINTBEGIN(*-to-static*)

struct some_impl {
    void foo() {
        std::println("some_impl");
    };
    void foo() const {
        std::println("foo const some_impl");
    };
    void foo() const volatile {
        std::println("foo const volatile some_impl");
    };
    void bar() {
        std::println("bar some_impl");
    };
};
struct other_impl_alt_base {
    using bar_t = decltype([] { std::println("lambda bar other_impl_alt_base"); });
    bar_t bar;
};
struct other_impl_base {
    // void bar(void*) const {
    //     std::println("bar other_impl_base");
    // };
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
    void foo() const volatile {
        std::println("foo const volatile other_impl");
    };
};
static_assert(trp::implements_trait<other_impl, testing::trait_proto>);

// NOLINTEND(*-to-static*)

int main() {
    std::println("shared");
    const volatile auto to = trp::make_shared_trait<testing::trait_proto, some_impl>();
    to.foo();
    // static_assert(trp::implements_trait<decltype(to), testing::trait_proto>); // nt working yet

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
