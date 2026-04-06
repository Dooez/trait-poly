#include "trp/shared_trait_ptr.hpp"

namespace testing {

struct base {
    void _ping();
    void c_ping() const;
    void v_ping() volatile;
    void cv_ping() const volatile;
};

struct midl : base {
    void _midl();
    void c_midl() const;
    void v_midl() volatile;
    void cv_midl() const volatile;
};

struct leaf : midl {
    void _leaf();
    void c_leaf() const;
    void v_leaf() volatile;
    void cv_leaf() const volatile;
};

struct susp {
    void _ping();
    void c_ping() const;
    void v_ping() volatile;
    void cv_ping() const volatile;
};
consteval {
    trp::define_trait<base>();
    trp::define_trait<midl>();
    trp::define_trait<leaf>();
    trp::define_trait<susp>();
}

// clang-format off

// all traits tested with these macros have non-cv methods, so no cv trait may be a supertrait to a trait
#define CHECK_CV_SUPER(Co, S, T) \
static_assert(    Co<               S,     T>); \
static_assert(    Co<const          S,     T>); \
static_assert(    Co<volatile       S,     T>); \
static_assert(    Co<const volatile S,     T>); \
\
static_assert(not Co<               S, const   T>); \ 
static_assert(    Co<const          S, const   T>); \
static_assert(not Co<volatile       S, const   T>); \
static_assert(    Co<const volatile S, const   T>); \
\
static_assert(not Co<               S, volatile   T>); \
static_assert(not Co<const          S, volatile   T>); \
static_assert(    Co<volatile       S, volatile   T>); \
static_assert(    Co<const volatile S, volatile   T>); \
\
static_assert(not Co<               S, const volatile T>); \
static_assert(not Co<const          S, const volatile T>); \
static_assert(not Co<volatile       S, const volatile T>); \
static_assert(    Co<const volatile S, const volatile T>);

#define CHECK_CV_NOT_SUPER(Co, S, T) \
static_assert(not Co<               S,     T>); \
static_assert(not Co<const          S,     T>); \
static_assert(not Co<volatile       S,     T>); \
static_assert(not Co<const volatile S,     T>); \
\
static_assert(not Co<               S, const   T>); \
static_assert(not Co<const          S, const   T>); \
static_assert(not Co<volatile       S, const   T>); \
static_assert(not Co<const volatile S, const   T>); \
\
static_assert(not Co<               S, volatile   T>); \
static_assert(not Co<const          S, volatile   T>); \
static_assert(not Co<volatile       S, volatile   T>); \
static_assert(not Co<const volatile S, volatile   T>); \
\
static_assert(not Co<               S, const volatile T>); \
static_assert(not Co<const          S, const volatile T>); \
static_assert(not Co<volatile       S, const volatile T>); \
static_assert(not Co<const volatile S, const volatile T>);

CHECK_CV_SUPER(trp::supertrait_of, leaf, leaf);
CHECK_CV_SUPER(trp::supertrait_of, midl, leaf);
CHECK_CV_SUPER(trp::supertrait_of, base, leaf);
CHECK_CV_SUPER(trp::supertrait_of, susp, leaf);

CHECK_CV_SUPER    (trp::explicit_supertrait_of, leaf, leaf);
CHECK_CV_SUPER    (trp::explicit_supertrait_of, midl, leaf);
CHECK_CV_SUPER    (trp::explicit_supertrait_of, base, leaf);
CHECK_CV_NOT_SUPER(trp::explicit_supertrait_of, susp, leaf);

CHECK_CV_NOT_SUPER(trp::direct_supertrait_of, leaf, leaf);
CHECK_CV_SUPER    (trp::direct_supertrait_of, midl, leaf);
CHECK_CV_NOT_SUPER(trp::direct_supertrait_of, base, leaf);
CHECK_CV_NOT_SUPER(trp::direct_supertrait_of, susp, leaf);
// clang-format on



struct trait_touch {
    void touch();
    void c_touch() const;
};
trp::define_trait<trait_touch>();

struct impl_all_cv {
    void ping() {}
    void c_ping() const {}
    void v_ping() volatile {}
    void cv_ping() const volatile {}

    void mid() {}
    void c_mid() const {}
    void v_mid() volatile {}
    void cv_mid() const volatile {}

    void leaf() {}
    void c_leaf() const {}
    void v_leaf() volatile {}
    void cv_leaf() const volatile {}
};

struct impl_touch {
    void touch() {}
    void c_touch() const {}
};

// static_assert(trp::implements_trait<impl_all_cv, testing::trait_leaf>);

static_assert(trp::implements_trait<impl_touch, trait_touch>);
static_assert(not trp::implements_trait<const impl_touch, trait_touch>);
static_assert(not trp::implements_trait<volatile impl_touch, trait_touch>);
static_assert(trp::implements_trait<const impl_touch, const trait_touch>);


}    // namespace testing

int main() {
    return 0;
}
