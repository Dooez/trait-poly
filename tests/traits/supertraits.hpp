#pragma once

#include "trp/shared_trait_ptr.hpp"

namespace supertraits {

template<int>
struct ping_methods {
    void _ping();
    void c_ping() const;
    void v_ping() volatile;
    void cv_ping() const volatile;
};

using base = ping_methods<0>;

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

using susp = ping_methods<1>;

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
static_assert(not trp::implements_trait<impl_touch const, trait_touch>);
static_assert(not trp::implements_trait<impl_touch volatile, trait_touch>);
static_assert(trp::implements_trait<impl_touch const, trait_touch const>);

}    // namespace supertraits
