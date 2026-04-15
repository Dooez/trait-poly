#include "trp/shared_trait_ptr.hpp"
#include "trp/unique_trait_ptr.hpp"

#include <print>
struct trait_proto_base {
    void bar();
};
struct trait_proto_imposter {
    void bar();
};
consteval {
    trp::define_trait<trait_proto_base>();
    trp::define_trait<trait_proto_imposter>();
}
struct trait_proto : trait_proto_base {
    void foo();
    void foo() const;
    void foo() const volatile;
};
consteval {
    trp::define_trait<trait_proto_base>();    // repeated definition is allowed, and has no effect
    trp::define_trait<trait_proto>();
}

static_assert(trp::supertrait_of<trait_proto, trait_proto>);
static_assert(trp::supertrait_of<trait_proto_base, trait_proto>);
static_assert(trp::supertrait_of<trait_proto_imposter, trait_proto>);

static_assert(trp::explicit_supertrait_of<trait_proto, trait_proto>);
static_assert(trp::explicit_supertrait_of<trait_proto_base, trait_proto>);
static_assert(not trp::explicit_supertrait_of<trait_proto_imposter, trait_proto>);

static_assert(not trp::direct_supertrait_of<trait_proto, trait_proto>);
static_assert(trp::direct_supertrait_of<trait_proto_base, trait_proto>);
static_assert(not trp::direct_supertrait_of<trait_proto_imposter, trait_proto>);

static_assert(not trp::supertrait_of<trait_proto, trait_proto_base>);
static_assert(not trp::explicit_supertrait_of<trait_proto, trait_proto_base>);
static_assert(not trp::direct_supertrait_of<trait_proto, trait_proto_base>);

// NOLINTBEGIN(*-to-static*)
struct trait_foo_only {
    void foo();
};
consteval {
    trp::define_trait<trait_foo_only>();
}
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
static_assert(trp::implements_trait<other_impl, trait_proto>);

// NOLINTEND(*-to-static*)
template<typename S>
constexpr bool compare_structs(const S& lhs, const S& rhs) {
    static constexpr auto mems = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^S, std::meta::access_context::unchecked()));
    template for (constexpr auto m: mems) {
        using mem_t = [:std::meta::type_of(m):];
        if constexpr (std::equality_comparable<mem_t>) {
            if (lhs.[:m:] != rhs.[:m:])
                return false;
        } else {
            return compare_structs(lhs.[:m:], rhs.[:m:]);
        }
    }
    return true;
};

constexpr auto vt0  = trp::detail::fill_vtable<trait_proto, some_impl>();
constexpr auto vt01 = *trp::detail::get_explicit_supertrait_vtable_ptr<trait_proto_base>(&vt0);
constexpr auto vt1  = trp::detail::fill_vtable<trait_proto_base, some_impl>();
constexpr auto vt2  = trp::detail::fill_vtable<trait_proto_base, other_impl>();

static_assert(compare_structs(vt01, vt1));
static_assert(not compare_structs(vt01, vt2));

int main() {
    std::println("make_shared:");
    const auto to = trp::make_shared_trait<trait_proto, some_impl>();
    const auto toref = *to;
    to->foo();
    toref.foo();
    auto to_up = trp::trait_cast<trait_proto_base>(to);
    to_up->bar();

    auto cvto = trp::make_shared_trait<const volatile trait_proto, some_impl>();
    cvto->foo();

    auto cto = trp::make_shared_trait<const trait_proto, some_impl>();
    cto->foo();
    cto = trp::make_shared_trait<const trait_proto, other_impl>();
    cto->foo();

    auto to2 = trp::make_shared_trait<trait_proto, other_impl>();

    to2->foo();
    to2->bar();

    // other_impl{}.bar();
    //
    std::println("\nmake_unique:");
    auto uptr = trp::make_unique_trait<trait_proto, some_impl>();
    uptr->foo();
    uptr->bar();
    uptr = trp::make_unique_trait<trait_proto, other_impl>();
    uptr->foo();

    std::println("\nalloc_unique:");
    auto auptr = trp::allocate_unique_trait<trait_proto, some_impl>(std::allocator<some_impl>{});
    auptr->foo();
    std::println("\nmove unique to alloc_unique:");
    auptr = std::move(uptr);
    auptr->foo();
    std::println("\ncast auptr to base:");
    auto auptr_up = trp::trait_cast<trait_proto_base>(std::move(auptr));
    auptr_up->bar();

    auto auptr2 = trp::allocate_unique_trait<const trait_proto, other_impl>(std::allocator<other_impl>{});
    auptr2->foo();

    std::println("\nref:");
    auto simpl = some_impl{};
    auto tref  = trp::trait_ref<trait_proto>(simpl);
    tref.foo();

    std::println("\nref upcast:");
    auto tref_up = trp::trait_cast<trait_proto_base>(tref);
    tref_up.bar();

    const auto csimpl = some_impl{};
    auto       tref2  = trp::trait_ref<trait_foo_only>(csimpl);    // implementation may be overqualified
    tref2.foo();

    return 0;
}
