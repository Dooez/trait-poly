#include "trp/shared_trait_ptr.hpp"
#include "trp/unique_trait_ptr.hpp"

#include <print>

struct no_def_ctor {
    no_def_ctor() = delete;
    explicit no_def_ctor(int) {};
};

struct trait_proto_imposter {
    auto bar() const -> no_def_ctor;
};
consteval {
    trp::define_trait<trait_proto_imposter>();
}
struct trait_proto_base {
    auto bar() const -> no_def_ctor;
};
struct trait_proto : trait_proto_base {
    void foo();
    void foo() const;
};
template<typename T>
struct some_default_impl {
    static void foo(const T& v) {
        std::print("[default const foo impl]");
        v.bar();
        // v.baz();
        std::println();
    };
    static void foo(T& v) {
        std::print("[default foo impl]");
        v.bar();
        std::println();
    };
};
consteval {
    trp::define_trait<trait_proto, some_default_impl>();
}
struct bar_only_impl {
    auto bar() const -> no_def_ctor {
        std::print("[bar_only_impl::bar]");
        return no_def_ctor{1};
    }
};
// consteval {
// trp::define_trait<trait_proto_base>();    // repeated definition is allowed, and has no effect
// trp::define_trait<trait_proto>();
// }

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
    void bar() const {
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

struct trait_foo_only {
    void foo();
};
struct trait_fb {
    void foo() const;
    void bar();
};
consteval {
    trp::define_trait<trait_foo_only>();
    trp::define_trait<trait_fb>();
}
struct foo_only_impl {
    void foo() const {
        std::println("foo foo_only_impl");
    };
};
// static_assert(trp::implements_trait<other_impl, trait_proto>);

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
//
// constexpr auto vt0  = trp::detail::fill_vtable<trait_proto, some_impl>();
// constexpr auto vt01 = *trp::detail::get_explicit_supertrait_vtable_ptr<trait_proto_base>(&vt0);
// constexpr auto vt1  = trp::detail::fill_vtable<trait_proto_base, some_impl>();
// constexpr auto vt2  = trp::detail::fill_vtable<trait_proto_base, other_impl>();
//
// static_assert(compare_structs(vt01, vt1));
// static_assert(not compare_structs(vt01, vt2));

// consteval {
//     trp::detail::define_cvts_ref<trait_proto, some_impl>();
// }


int main() {
    auto simpl = bar_only_impl{};
    auto ref   = trp::dyn_trait_ref<const trait_proto>(simpl);
    ref.foo();
    std::println("\n---");
    ref.bar();
    std::println("\n---");

    const auto ref2 = trp::dyn_trait_ref<trait_proto>(simpl);
    ref2.foo();
    std::println("\n---");
    // ref.bar();
    // auto       ts_ref   = trp::detail::cvts_trait_ref<trait_proto, some_impl>(simpl);
    // const auto ts_ref_c = trp::detail::cvts_trait_ref<trait_proto, some_impl>(simpl);
    // ts_ref.foo();
    // ts_ref_c.foo();


    // std::println("make_shared:");
    // const auto to    = trp::make_shared_trait<trait_proto, some_impl>();
    // const auto toref = *to;
    // to->foo();
    // toref.foo();
    // auto to_up = trp::trait_cast<trait_proto_base>(to);
    // to_up->bar();
    //
    // auto cvto = trp::make_shared_trait<const volatile trait_proto, some_impl>();
    // cvto->foo();
    // std::println();
    //
    // std::println("/* dyn_trait_ref<const volatile trait_proto> */ x.foo();");
    // auto cvtoref = *cvto;
    // auto(cvtoref).foo();
    // std::println("Can cast to non-const trait_proto: {}",
    //              trp::is_valid_const_trait_cast<trait_proto>(cvtoref));
    // std::println("const_trait_cast<trait_proto>(x).foo()");
    // trp::const_trait_cast<trait_proto>(cvtoref).foo();
    // std::println();
    //
    // auto cto = trp::make_shared_trait<const trait_proto, some_impl>();
    // cto->foo();
    // cto = trp::make_shared_trait<const trait_proto, other_impl>();
    // cto->foo();
    //
    // auto to2 = trp::make_shared_trait<trait_proto, other_impl>();
    //
    // to2->foo();
    // to2->bar();
    //
    // // other_impl{}.bar();
    // //
    // std::println("\nmake_unique:");
    // auto uptr = trp::make_unique_trait<trait_proto, some_impl>();
    // uptr->foo();
    // uptr->bar();
    // uptr = trp::make_unique_trait<trait_proto, other_impl>();
    // uptr->foo();
    //
    // std::println("\nalloc_unique:");
    // auto auptr = trp::allocate_unique_trait<trait_proto, some_impl>(std::allocator<some_impl>{});
    // auptr->foo();
    // std::println("\nmove unique to alloc_unique:");
    // auptr = std::move(uptr);
    // auptr->foo();
    // std::println("\ncast auptr to base:");
    // auto auptr_up = trp::trait_cast<trait_proto_base>(std::move(auptr));
    // auptr_up->bar();
    //
    // auto auptr2 = trp::allocate_unique_trait<const trait_proto, other_impl>(std::allocator<other_impl>{});
    // auptr2->foo();
    //
    //
    // std::println("\nref:");
    // const auto simpl = foo_only_impl{};
    // auto       tref  = trp::dyn_trait_ref<const trait_fb>(simpl);
    // tref.foo();
    // std::println("Can cast to non-const trait_fb: {}", trp::is_valid_const_trait_cast<trait_fb>(tref));
    //
    // const auto csimpl = some_impl{};
    // auto       tref2  = trp::dyn_trait_ref<trait_foo_only>(csimpl);    // implementation may be overqualified
    // tref2.foo();

    return 0;
}
