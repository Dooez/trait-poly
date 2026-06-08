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
struct trait_proto_base {
    auto        bar() const -> no_def_ctor;
    auto        bar(int) const -> no_def_ctor;
    static auto bar(auto const& v) -> no_def_ctor {
        std::println("[default trait_proto_base::bar]");
        return no_def_ctor{1};
    };
    static auto bar(auto const& v, int) -> no_def_ctor {
        std::println("[default trait_proto_base::bar(int)]");
        return no_def_ctor{1};
    };
};
struct trait_proto : trait_proto_base {
    static void foo(auto const& v) {
        std::print("[default const foo impl]");
        v.bar();
        // v.baz();
        std::println();
    };
    static void foo(auto& v) {
        std::print("[default foo impl]");
        v.bar();
        std::println();
    };
    static auto bar(auto const& v) -> no_def_ctor {
        std::println("[default trait_proto::bar]");
        return no_def_ctor{1};
    };
    void foo();
    void foo() const;
};
struct bar_only_impl {
    auto bar() const -> no_def_ctor {
        std::print("[bar_only_impl::bar]");
        return no_def_ctor{1};
    }
    auto bar(int a) const -> no_def_ctor {
        std::print("[bar_only_impl::bar(int)]");
        return no_def_ctor{1};
    }
    auto not_a_trait_foo() const {
        std::print("[bar_only_impl::not_a_trait_foo()]");
    }
};
template<>
struct trp::impl_spec_for<bar_only_impl, trait_proto> {
    static auto bar(auto const& i) -> no_def_ctor {
        static_cast<bar_only_impl const&>(i).not_a_trait_foo();
        std::print("[impl_spec_for<bar_only_impl, trait_proto>::bar()]");
        return no_def_ctor{1};
    }
    // static auto bar(const bar_only_impl& i) -> no_def_ctor {
    //     i.not_a_trait_foo();
    //     std::print("[impl_spec_for<bar_only_impl, trait_proto>::bar()]");
    //     return no_def_ctor{1};
    // }
};

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
struct foo_only_impl {
    void foo() const {
        std::println("foo foo_only_impl");
    };
};

template<>
struct trp::default_impl_spec<trait_proto> {
    static auto bar(auto const& v) -> no_def_ctor {
        std::println("[default specialization for trait_proto::bar]");
        return no_def_ctor{1};
    };
};
static_assert(trp::implements_trait<bar_only_impl, trait_proto>);

int main() {
    auto simpl = bar_only_impl{};

    auto ref = trp::dyn_trait_ref<trait_proto const>(simpl);
    ref.foo();
    std::println("\n---");
    ref.bar();
    std::println("\n---");

    auto const ref2 = trp::dyn_trait_ref<trait_proto>(simpl);
    ref2.foo();
    std::println("\n---");

    auto em_shptr = trp::shared_trait_ptr<trait_proto>{};


    // //
    std::println("\nmake_unique:");
    auto uptr = trp::make_unique_trait<trait_proto, some_impl>();
    uptr->foo();
    uptr->bar();
    uptr = trp::make_unique_trait<trait_proto, other_impl>();
    uptr->foo();
    auto rawvoidptr = uptr.get();
    auto rawptr     = uptr.get<other_impl>();
    rawptr->foo();
    //

    return 0;
}
