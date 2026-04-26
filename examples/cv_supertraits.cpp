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
}    // namespace testing

namespace testing {

// clang-format off

// all traits tested with these macros have non-cv methods, so no cv trait may be a supertrait to a trait
#define CHECK_CV_SUPER(Concept, S, T) \
static_assert(    Concept<               S,     T>); \
static_assert(    Concept<const          S,     T>); \
static_assert(    Concept<volatile       S,     T>); \
static_assert(    Concept<const volatile S,     T>); \
\
static_assert(not Concept<               S, const   T>); \
static_assert(    Concept<const          S, const   T>); \
static_assert(not Concept<volatile       S, const   T>); \
static_assert(    Concept<const volatile S, const   T>); \
\
static_assert(not Concept<               S, volatile   T>); \
static_assert(not Concept<const          S, volatile   T>); \
static_assert(    Concept<volatile       S, volatile   T>); \
static_assert(    Concept<const volatile S, volatile   T>); \
\
static_assert(not Concept<               S, const volatile T>); \
static_assert(not Concept<const          S, const volatile T>); \
static_assert(not Concept<volatile       S, const volatile T>); \
static_assert(    Concept<const volatile S, const volatile T>);

#define CHECK_CV_NOT_SUPER(Concept, S, T) \
static_assert(not Concept<               S,     T>); \
static_assert(not Concept<const          S,     T>); \
static_assert(not Concept<volatile       S,     T>); \
static_assert(not Concept<const volatile S,     T>); \
\
static_assert(not Concept<               S, const   T>); \
static_assert(not Concept<const          S, const   T>); \
static_assert(not Concept<volatile       S, const   T>); \
static_assert(not Concept<const volatile S, const   T>); \
\
static_assert(not Concept<               S, volatile   T>); \
static_assert(not Concept<const          S, volatile   T>); \
static_assert(not Concept<volatile       S, volatile   T>); \
static_assert(not Concept<const volatile S, volatile   T>); \
\
static_assert(not Concept<               S, const volatile T>); \
static_assert(not Concept<const          S, const volatile T>); \
static_assert(not Concept<volatile       S, const volatile T>); \
static_assert(not Concept<const volatile S, const volatile T>);

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


// cv qualification may preserve trait equivalence
struct ping_cv {
    void cv_ping() const volatile;
};
}
namespace testing{
static_assert(trp::supertrait_of<ping_cv,                ping_cv>);
static_assert(trp::supertrait_of<const ping_cv,          ping_cv>);
static_assert(trp::supertrait_of<volatile ping_cv,       ping_cv>);
static_assert(trp::supertrait_of<const volatile ping_cv, ping_cv>);

static_assert(trp::supertrait_of<ping_cv,                const ping_cv>);
static_assert(trp::supertrait_of<const ping_cv,          const ping_cv>);
static_assert(trp::supertrait_of<volatile ping_cv,       const ping_cv>);
static_assert(trp::supertrait_of<const volatile ping_cv, const ping_cv>);

static_assert(trp::supertrait_of<ping_cv,                volatile ping_cv>);
static_assert(trp::supertrait_of<const ping_cv,          volatile ping_cv>);
static_assert(trp::supertrait_of<volatile ping_cv,       volatile ping_cv>);
static_assert(trp::supertrait_of<const volatile ping_cv, volatile ping_cv>);

static_assert(trp::supertrait_of<ping_cv,                const volatile ping_cv>);
static_assert(trp::supertrait_of<const ping_cv,          const volatile ping_cv>);
static_assert(trp::supertrait_of<volatile ping_cv,       const volatile ping_cv>);
static_assert(trp::supertrait_of<const volatile ping_cv, const volatile ping_cv>);


// clang-format on


struct trait_touch {
    void touch();
    void c_touch() const;
};
}    // namespace testing
namespace testing {
struct impl_all_cv {
    void _ping() {}
    void c_ping() const {}
    void v_ping() volatile {}
    void cv_ping() const volatile {}

    void _midl() {}
    void c_midl() const {}
    void v_midl() volatile {}
    void cv_midl() const volatile {}

    void _leaf() {}
    void c_leaf() const {}
    void v_leaf() volatile {}
    void cv_leaf() const volatile {}
};

struct impl_touch {
    void touch() {}
    void c_touch() const {}
};

static_assert(trp::implements_trait<impl_all_cv, leaf>);

static_assert(trp::implements_trait<impl_touch, trait_touch>);
static_assert(not trp::implements_trait<const impl_touch, trait_touch>);
static_assert(not trp::implements_trait<volatile impl_touch, trait_touch>);
static_assert(trp::implements_trait<const impl_touch, const trait_touch>);

}    // namespace testing

int main() {
    return 0;
}
