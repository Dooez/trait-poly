#pragma once

#include "support/test_support.hpp"
#include "trait_ptr/support.hpp"
#include "trp/shared_trait_ptr.hpp"
#include "trp/unique_trait_ptr.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace fancy_allocator {

inline constexpr auto case_name = std::string_view("fancy_allocator");

struct allocator_counts {
    int allocations   = 0;
    int deallocations = 0;
};

template<typename T>
struct fancy_pointer {
    using element_type    = T;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    using rebind = fancy_pointer<U>;

    T* raw = nullptr;

    constexpr fancy_pointer() = default;
    constexpr explicit fancy_pointer(T* value)
    : raw(value) {}

    template<typename U>
        requires std::convertible_to<U*, T*>
    constexpr fancy_pointer(fancy_pointer<U> other)
    : raw(other.raw) {}

    constexpr auto operator*() const -> T& {
        return *raw;
    }

    constexpr auto operator->() const -> T* {
        return raw;
    }

    friend constexpr auto operator==(fancy_pointer, fancy_pointer) -> bool = default;
};

template<typename T>
struct fancy_allocator {
    using value_type = T;
    using pointer    = fancy_pointer<T>;

    allocator_counts* counts = nullptr;

    fancy_allocator() = default;
    explicit fancy_allocator(allocator_counts& value)
    : counts(&value) {}

    template<typename U>
    fancy_allocator(fancy_allocator<U> const& other)
    : counts(other.counts) {}

    auto allocate(std::size_t count) -> pointer {
        ++counts->allocations;
        return pointer(static_cast<T*>(::operator new(count * sizeof(T))));
    }

    void deallocate(pointer ptr, std::size_t) {
        ++counts->deallocations;
        ::operator delete(ptr.raw);
    }

    friend auto operator==(fancy_allocator, fancy_allocator) -> bool = default;
};

struct value_impl {
    int stored;

    auto value() const -> int {
        return stored;
    }
};

using construction_error = valuetest::construction_error;

struct throwing_impl {
    explicit throwing_impl(int) {
        throw construction_error{};
    }

    auto value() const -> int {
        return 0;
    }
};

using aligned_impl = valuetest::aligned_impl;

static_assert(alignof(aligned_impl) > alignof(std::max_align_t));

void check_successful_allocation() {
    auto counts = allocator_counts{};
    auto alloc  = fancy_allocator<std::byte>(counts);

    {
        auto shared = trp::allocate_shared_trait<read_trait, value_impl>(alloc, 1);
        auto unique = trp::allocate_unique_trait<read_trait, value_impl>(alloc, 2);

        test::expect_eq(case_name, "shared value", shared->value(), 1);
        test::expect_eq(case_name, "unique value", unique->value(), 2);
        test::expect_eq(case_name, "allocation count", counts.allocations, 2);
    }

    test::expect_eq(case_name, "deallocation count", counts.deallocations, 2);
}

void check_throwing_construction() {
    auto counts = allocator_counts{};
    auto alloc  = fancy_allocator<std::byte>(counts);

    try {
        (void)trp::allocate_unique_trait<read_trait, throwing_impl>(alloc, 1);
        test::fail(case_name, "throwing unique construction should fail");
    } catch (construction_error const&) {}
    test::expect_eq(case_name, "throwing unique allocation count", counts.allocations, 1);
    test::expect_eq(case_name, "throwing unique deallocation count", counts.deallocations, 1);

    try {
        (void)trp::allocate_shared_trait<read_trait, throwing_impl>(alloc, 2);
        test::fail(case_name, "throwing shared construction should fail");
    } catch (construction_error const&) {}
    test::expect_eq(case_name, "throwing shared allocation count", counts.allocations, 2);
    test::expect_eq(case_name, "throwing shared deallocation count", counts.deallocations, 2);
}

void check_overaligned_allocation() {
    auto counts = allocator_counts{};
    auto alloc  = fancy_allocator<std::byte>(counts);

    {
        auto unique = trp::allocate_unique_trait<read_trait, aligned_impl>(alloc, 3);
        auto shared = trp::allocate_shared_trait<read_trait, aligned_impl>(alloc, 4);

        auto const unique_address = reinterpret_cast<std::uintptr_t>(unique.get<aligned_impl>());
        auto const shared_address = reinterpret_cast<std::uintptr_t>(shared.get<aligned_impl>());
        test::expect_eq(case_name,
                        "overaligned unique address",
                        unique_address % alignof(aligned_impl),
                        std::uintptr_t{0});
        test::expect_eq(case_name,
                        "overaligned shared address",
                        shared_address % alignof(aligned_impl),
                        std::uintptr_t{0});
        test::expect_eq(case_name, "overaligned unique value", unique->value(), 3);
        test::expect_eq(case_name, "overaligned shared value", shared->value(), 4);
    }

    test::expect_eq(case_name, "overaligned allocation count", counts.allocations, 2);
    test::expect_eq(case_name, "overaligned deallocation count", counts.deallocations, 2);
}

inline void run() {
    check_successful_allocation();
    check_throwing_construction();
    check_overaligned_allocation();
}

}    // namespace fancy_allocator
