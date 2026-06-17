#include "trp/dyn_trait_ref.hpp"

#include "test_support.hpp"

struct cv_surface_trait {
    auto full_only() -> int;
    auto const_only() const -> int;
    auto volatile_only() volatile -> int;
    auto cv_only() const volatile -> int;
};

struct cv_surface_impl {
    auto full_only() -> int {
        return 10;
    }

    auto const_only() const -> int {
        return 20;
    }

    auto volatile_only() volatile -> int {
        return 30;
    }

    auto cv_only() const volatile -> int {
        return 40;
    }
};

template<typename Ref>
concept can_call_full_only = requires(Ref ref) { ref.full_only(); };

template<typename Ref>
concept can_call_const_only = requires(Ref ref) { ref.const_only(); };

template<typename Ref>
concept can_call_volatile_only = requires(Ref ref) { ref.volatile_only(); };

template<typename Ref>
concept can_call_cv_only = requires(Ref ref) { ref.cv_only(); };

using cv_full_ref = trp::dyn_trait_ref<cv_surface_trait>;
using cv_c_ref    = trp::dyn_trait_ref<cv_surface_trait const>;
using cv_v_ref    = trp::dyn_trait_ref<cv_surface_trait volatile>;
using cv_cv_ref   = trp::dyn_trait_ref<cv_surface_trait const volatile>;

static_assert(can_call_full_only<cv_full_ref>);
static_assert(can_call_const_only<cv_full_ref>);
static_assert(can_call_volatile_only<cv_full_ref>);
static_assert(can_call_cv_only<cv_full_ref>);

static_assert(!can_call_full_only<cv_c_ref>);
static_assert(can_call_const_only<cv_c_ref>);
static_assert(!can_call_volatile_only<cv_c_ref>);
static_assert(can_call_cv_only<cv_c_ref>);

static_assert(!can_call_full_only<cv_v_ref>);
static_assert(!can_call_const_only<cv_v_ref>);
static_assert(can_call_volatile_only<cv_v_ref>);
static_assert(can_call_cv_only<cv_v_ref>);

static_assert(!can_call_full_only<cv_cv_ref>);
static_assert(!can_call_const_only<cv_cv_ref>);
static_assert(!can_call_volatile_only<cv_cv_ref>);
static_assert(can_call_cv_only<cv_cv_ref>);

struct overload_trait {
    auto value() -> int;
    auto value() const -> int;
    auto value() volatile -> int;
    auto value() const volatile -> int;
};

struct overload_impl {
    int full_calls = 0;

    auto value() -> int {
        ++full_calls;
        return 1;
    }

    auto value() const -> int {
        return 2;
    }

    auto value() volatile -> int {
        return 3;
    }

    auto value() const volatile -> int {
        return 4;
    }
};

struct const_cast_trait {
    auto read() const -> int;
    auto write(int value) -> void;
};

struct const_cast_full_impl {
    int value = 5;

    auto read() const -> int {
        return value;
    }

    auto write(int new_value) -> void {
        value = new_value;
    }
};

struct const_cast_readonly_impl {
    int value = 7;

    auto read() const -> int {
        return value;
    }
};

template<typename Ref>
concept can_call_write = requires(Ref ref) { ref.write(0); };

static_assert(trp::implements_trait<const_cast_readonly_impl, const_cast_trait const>);
static_assert(!trp::implements_trait<const_cast_readonly_impl, const_cast_trait>);
static_assert(!can_call_write<trp::dyn_trait_ref<const_cast_trait const>>);

struct readable_trait {
    auto read() const -> int;
};

struct writable_trait : readable_trait {
    auto write(int value) -> void;
};

struct writable_impl {
    int value = 11;

    auto read() const -> int {
        return value;
    }

    auto write(int new_value) -> void {
        value = new_value;
    }
};

struct other_readable_impl {
    auto read() const -> int {
        return 99;
    }
};

struct other_writable_impl {
    auto read() const -> int {
        return 77;
    }

    auto write(int) -> void {}
};

void check_cv_surface_filters() {
    auto impl = cv_surface_impl{};

    auto full_ref = cv_full_ref(impl);
    test::expect_eq("dyn_trait_ref", "full ref calls full method", full_ref.full_only(), 10);
    test::expect_eq("dyn_trait_ref", "full ref calls const method", full_ref.const_only(), 20);
    test::expect_eq("dyn_trait_ref", "full ref calls volatile method", full_ref.volatile_only(), 30);
    test::expect_eq("dyn_trait_ref", "full ref calls cv method", full_ref.cv_only(), 40);

    auto const_ref = cv_c_ref(impl);
    test::expect_eq("dyn_trait_ref", "const ref calls const method", const_ref.const_only(), 20);
    test::expect_eq("dyn_trait_ref", "const ref calls cv method", const_ref.cv_only(), 40);

    auto volatile_ref = cv_v_ref(impl);
    test::expect_eq("dyn_trait_ref", "volatile ref calls volatile method", volatile_ref.volatile_only(), 30);
    test::expect_eq("dyn_trait_ref", "volatile ref calls cv method", volatile_ref.cv_only(), 40);

    auto cv_ref = cv_cv_ref(impl);
    test::expect_eq("dyn_trait_ref", "cv ref calls cv method", cv_ref.cv_only(), 40);
}

void check_cv_overload_dispatch() {
    auto impl = overload_impl{};

    auto full_ref = trp::dyn_trait_ref<overload_trait>(impl);
    test::expect_eq("dyn_trait_ref", "full overload selected", full_ref.value(), 1);
    test::expect_eq("dyn_trait_ref", "full overload call recorded", impl.full_calls, 1);

    auto const_ref = trp::dyn_trait_ref<overload_trait const>(impl);
    test::expect_eq("dyn_trait_ref", "const overload selected", const_ref.value(), 2);
    test::expect_eq("dyn_trait_ref", "const overload does not call full", impl.full_calls, 1);

    auto volatile_ref = trp::dyn_trait_ref<overload_trait volatile>(impl);
    test::expect_eq("dyn_trait_ref", "volatile overload selected", volatile_ref.value(), 3);
    test::expect_eq("dyn_trait_ref", "volatile overload does not call full", impl.full_calls, 1);

    auto cv_ref = trp::dyn_trait_ref<overload_trait const volatile>(impl);
    test::expect_eq("dyn_trait_ref", "cv overload selected", cv_ref.value(), 4);
    test::expect_eq("dyn_trait_ref", "cv overload does not call full", impl.full_calls, 1);
}

void check_const_trait_cast_precheck() {
    auto full_impl = const_cast_full_impl{};
    auto full_ref  = trp::dyn_trait_ref<const_cast_trait const>(full_impl);

    test::expect_eq("dyn_trait_ref",
                    "full impl const cast is valid",
                    trp::is_valid_const_trait_cast<const_cast_trait>(full_ref),
                    true);

    auto writable_ref = trp::const_trait_cast<const_cast_trait>(full_ref);
    writable_ref.write(17);
    test::expect_eq("dyn_trait_ref", "checked const trait cast reaches writable impl", full_impl.value, 17);

    auto readonly_impl = const_cast_readonly_impl{};
    auto readonly_ref  = trp::dyn_trait_ref<const_cast_trait const>(readonly_impl);

    test::expect_eq("dyn_trait_ref",
                    "readonly impl const cast is rejected",
                    trp::is_valid_const_trait_cast<const_cast_trait>(readonly_ref),
                    false);
    test::expect_eq("dyn_trait_ref", "readonly ref still reads", readonly_ref.read(), 7);
}

void check_type_checks_and_trait_casts() {
    auto impl = writable_impl{};
    auto ref  = trp::dyn_trait_ref<writable_trait>(impl);

    test::expect_eq(
        "dyn_trait_ref", "holding exact writable impl", trp::is_holding_type<writable_impl>(ref), true);
    test::expect_eq(
        "dyn_trait_ref", "not holding other impl", trp::is_holding_type<other_writable_impl>(ref), false);

    auto readable_ref = trp::trait_cast<readable_trait>(ref);
    test::expect_eq("dyn_trait_ref", "explicit supertrait reads", readable_ref.read(), 11);
    test::expect_eq("dyn_trait_ref",
                    "supertrait keeps runtime type",
                    trp::is_holding_type<writable_impl>(readable_ref),
                    true);

    if (trp::is_holding_type<writable_impl>(readable_ref)) {
        auto writable_ref = trp::trait_cast<writable_trait, writable_impl>(readable_ref);
        writable_ref.write(23);
    }
    test::expect_eq("dyn_trait_ref", "prechecked dyn trait cast writes", impl.value, 23);

    auto other_impl = other_readable_impl{};
    auto other_ref  = trp::dyn_trait_ref<readable_trait>(other_impl);
    test::expect_eq("dyn_trait_ref",
                    "wrong impl precheck rejects unsafe cast",
                    trp::is_holding_type<writable_impl>(other_ref),
                    false);
    test::expect_eq("dyn_trait_ref", "wrong impl remains readable", other_ref.read(), 99);
}

int main() {
    check_cv_surface_filters();
    check_cv_overload_dispatch();
    check_const_trait_cast_precheck();
    check_type_checks_and_trait_casts();

    return 0;
}
