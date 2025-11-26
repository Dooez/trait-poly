#include "testing.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <meta>
#include <ranges>
#include <utility>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = std::int64_t;

using u8       = uint8_t;
using u32      = uint32_t;
using u64      = std::uint64_t;
using uZ       = std::size_t;
using i64      = std::int64_t;
using iZ       = ssize_t;
using f32      = float;
using f64      = double;
namespace stdr = std::ranges;
namespace stdv = std::views;

template<auto V>
struct nontype {};

namespace detail {

using arc_t = std::atomic<uint64_t>;

template<typename Impl>
consteval auto get_align() {
    static_assert(alignof(void*) == alignof(uZ));
    static_assert(alignof(arc_t) >= alignof(void*));
    return std::max({alignof(Impl), alignof(void*), alignof(arc_t)});
};

struct signed_offsets {
    static constexpr iZ memstart   = -sizeof(arc_t) - sizeof(void*) * 2 - sizeof(uZ);
    static constexpr iZ memsize    = -sizeof(arc_t) - sizeof(void*) - sizeof(uZ);
    static constexpr iZ dtor       = -sizeof(arc_t) - sizeof(void*);
    static constexpr iZ arc        = -sizeof(arc_t);
    static constexpr iZ impl       = 0;
    static constexpr iZ ctrl_block = memstart;
};

struct alignas(sizeof(void*) * 2) shared_manager {
    using efptr = void*;
    const efptr* vtable_begin{};
    void*        obj_ptr{};

    shared_manager(const efptr* vtable_begin, void* obj_ptr)
    : vtable_begin(vtable_begin)
    , obj_ptr(obj_ptr) {
        increment();
    };


    auto obj() const -> std::byte* {
        return static_cast<std::byte*>(obj_ptr);
    }

    void increment() const {
        if (obj_ptr == nullptr)
            return;
        auto& counter = *reinterpret_cast<arc_t*>(obj() + signed_offsets::arc);
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    void decrement() const {
        if (obj_ptr == nullptr)
            return;
        auto& counter = *reinterpret_cast<arc_t*>(obj() + signed_offsets::arc);
        if (counter.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            using dtor_t         = void (*)(void*);
            auto* dtor           = *reinterpret_cast<dtor_t*>(obj() + signed_offsets::dtor);
            auto* ctrl_block_ptr = *reinterpret_cast<void**>(obj() + signed_offsets::ctrl_block);
            dtor(ctrl_block_ptr);
        };
    }
    shared_manager() = default;
    shared_manager(const shared_manager& other) {
        other.increment();
        vtable_begin = other.vtable_begin;
        obj_ptr      = other.obj_ptr;
    }
    shared_manager(shared_manager&& other) {
        vtable_begin       = other.vtable_begin;
        obj_ptr            = other.obj_ptr;
        other.vtable_begin = nullptr;
        other.obj_ptr      = nullptr;
    }
    shared_manager& operator=(const shared_manager& other) {
        decrement();
        other.increment();
        vtable_begin = other.vtable_begin;
        obj_ptr      = other.obj_ptr;
        return *this;
    }
    shared_manager& operator=(shared_manager&& other) {
        vtable_begin       = other.vtable_begin;
        obj_ptr            = other.obj_ptr;
        other.vtable_begin = nullptr;
        other.obj_ptr      = nullptr;
        return *this;
    }
    ~shared_manager() {
        decrement();
    }
};
template<typename T>
struct trait_traits {
    static constexpr auto is_type = std::meta::is_type(^^T);
    static constexpr auto no_data_members =
        std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).empty();
    static constexpr auto methods = [] {
        using namespace std;
        using namespace std::meta;
        constexpr auto n = [] {
            auto trait_members = members_of(^^T, access_context::current())            //
                                 | stdv::filter(not_fn(is_special_member_function))    //
                                 | stdr::to<vector<info>>();
            return trait_members.size();
        }();
        auto methods = array<info, n>{};
        stdr::copy(members_of(^^T, access_context::current())    //
                       | stdv::filter(not_fn(is_special_member_function)),
                   methods.begin());
        stdr::sort(methods, {}, identifier_of);
        return methods;
    }();
};
template<typename Trait>
struct trait_impl;
}    // namespace detail

template<typename T>
class shared_trait : public detail::trait_impl<T> {
    [[no_unique_address]] detail::shared_manager manager_;

public:
    shared_trait() = default;
    shared_trait(detail::shared_manager manager)
    : manager_(std::move(manager)) {};

    operator bool() {
        return manager_.obj_ptr != nullptr;
    }
};

namespace detail {

template<typename TraitProto, typename Impl, typename Allocator>
struct alignas(get_align<Impl>()) ctrl_block {
    static constexpr auto ialign   = get_align<Impl>();
    static constexpr auto hdr_size = sizeof(void*) * 2 + sizeof(uZ) + sizeof(arc_t);
    static constexpr auto misalign = (hdr_size + ialign - 1) / ialign * ialign - hdr_size;
    using dtor_ptr                 = void (*)(void*);

    std::byte* memstart_;
    uZ         n_;
    dtor_ptr   destructor_ptr_;
    arc_t      counter_{};
    Impl       impl_;

    [[no_unique_address]] Allocator allocator_;

    static void destroy(void* ctrl_ptr) {
        auto& ctrl      = *static_cast<ctrl_block*>(ctrl_ptr);
        auto  allocator = ctrl.allocator_;
        auto  memstart  = ctrl.memstart_;
        auto  n         = ctrl.n_;
        ctrl.~ctrl_block();
        std::allocator_traits<Allocator>::deallocate(allocator, memstart, n);
    }
};


template<uZ Index, typename Ret, typename... Args>
struct method_spec_t {};
template<typename T>
concept method_spec =
    std::meta::has_template_arguments(^^T) && std::meta::template_of(^^T) == ^^method_spec_t;
template<method_spec Spec>
struct overload_invoker;
template<uZ Index, typename Ret, typename... Args>
struct overload_invoker<method_spec_t<Index, Ret, Args...>> {
    auto invoke(Args&&... args) const -> Ret {
        const auto& mngr       = *reinterpret_cast<const detail::shared_manager*>(this);
        using func_t           = auto(void*, Args&&...)->Ret;
        using wrapper_fptr_t   = func_t*;
        const auto erased_ptr  = mngr.vtable_begin + Index;
        const auto wrapper_ptr = *reinterpret_cast<const wrapper_fptr_t*>(erased_ptr);
        return wrapper_ptr(mngr.obj_ptr, std::forward<Args>(args)...);
    }
};

template<method_spec... Specs>
struct method_invoker : overload_invoker<Specs>... {
    template<typename... CallArgs>
    auto operator()(CallArgs&&... args) {
        return try_invoke<0>(std::forward<CallArgs>(args)...);
    }
    template<uZ OverloadIdx, typename... CallArgs>
    auto try_invoke(CallArgs&&... args) {
        using overload_t = overload_invoker<Specs...[OverloadIdx]>;
        if constexpr (OverloadIdx >= sizeof...(Specs)) {
            static_assert(false, "No overload found");
        } else if constexpr (requires(CallArgs&&... args) {
                                 overload_t{}.invoke(std::forward<CallArgs>(args)...);
                             }) {
            return overload_t::invoke(std::forward<CallArgs>(args)...);
        } else {
            return try_invoke<OverloadIdx + 1>(std::forward<CallArgs>(args)...);
        }
    }
};

template<typename Impl, std::meta::info Method, typename Ret, typename... Args>
auto invoke_wrapper(void* impl_ptr, Args&&... args) -> Ret {
    auto& impl = *static_cast<Impl*>(impl_ptr);
    return impl.[:Method:](std::forward<Args>(args)...);
}


template<typename TraitProto, typename Impl>
auto fill_vtable() {
    using namespace std;
    using namespace std::meta;
    // check input trait prototype
    //
    using ttt                    = trait_traits<TraitProto>;
    constexpr auto wrapper_infos = [] {
        constexpr auto t_methods = ttt::methods;
        constexpr auto ctx       = access_context::current();

        auto impl_members = members_of(^^Impl, ctx)    //
                            | stdv::filter(not_fn(is_special_member_function));
        auto new_members = vector<info>{};
        for (auto mem: t_methods) {
            auto trait_name         = identifier_of(mem);
            auto trait_ret          = return_type_of(mem);
            auto trait_params_types = parameters_of(mem) | stdv::transform(type_of);
            for (auto imem: impl_members) {
                auto impl_name         = identifier_of(imem);
                auto impl_ret          = return_type_of(imem);
                auto impl_params_types = parameters_of(imem) | stdv::transform(type_of);
                if (impl_name == trait_name     //
                    && impl_ret == trait_ret    //
                    && stdr::equal(trait_params_types, impl_params_types)) {
                    new_members.push_back(imem);
                    break;
                }
            }
        }
        auto wrappers        = array<info, t_methods.size()>{};
        auto wrapper_tparams = vector<info>{};
        for (auto [wr, member]: stdv::zip(wrappers, new_members)) {
            wrapper_tparams.clear();
            wrapper_tparams.push_back(^^Impl);
            wrapper_tparams.push_back(reflect_constant(member));
            wrapper_tparams.push_back(return_type_of(member));
            wrapper_tparams.append_range(parameters_of(member) | stdv::transform(type_of));
            wr = substitute(^^invoke_wrapper, wrapper_tparams);
        }
        return wrappers;
    }();

    auto [... Is] = []<uZ... Is>(index_sequence<Is...>) {
        return make_tuple(integral_constant<uZ, Is>{}...);
    }(make_index_sequence<wrapper_infos.size()>{});
    return array{reinterpret_cast<void*>(&([:wrapper_infos[Is]:]))...};
}

template<typename TraitProto, typename Impl>
struct trait_vtable {
    static inline const auto value = fill_vtable<TraitProto, Impl>();
};
}    // namespace detail

constexpr void comptime_error();
template<typename TraitProto>
consteval void define_trait() {
    using namespace std;
    using namespace std::meta;
    using namespace ::detail;
    // check input trait prototype
    //
    constexpr auto ctx      = access_context::current();
    using ttt               = trait_traits<TraitProto>;
    auto new_members_raw    = vector<pair<string_view, vector<info>>>{};
    auto methods            = vector<info>{};
    auto method_spec_params = vector<info>{};
    uZ   i                  = 0;
    for (auto mem: ttt::methods) {
        method_spec_params.clear();
        method_spec_params.push_back(reflect_constant(i));
        method_spec_params.push_back(return_type_of(mem));
        method_spec_params.append_range(parameters_of(mem) | stdv::transform(type_of));
        const auto spec = substitute(^^method_spec_t, method_spec_params);
        auto it = stdr::find_if(new_members_raw, [=](auto& p) { return p.first == identifier_of(mem); });
        if (it == new_members_raw.end()) {
            new_members_raw.push_back({identifier_of(mem), {spec}});
        } else {
            it->second.push_back(spec);
        }
        ++i;
    }
    for (auto& [name, specs_vec]: new_members_raw) {
        const auto options      = data_member_options{.name = name, .no_unique_address = true};
        const auto invoker_type = substitute(^^method_invoker, specs_vec);

        methods.push_back(data_member_spec(invoker_type, options));
    }
    define_aggregate(^^trait_impl<TraitProto>, methods);
};

template<typename TraitProto, typename Impl, typename Alloc, typename... Args>
auto make_shared_trait(std::allocator_arg_t, const Alloc& allocator, Args&&... args) {
    using alloc        = std::allocator_traits<Alloc>::template rebind_alloc<std::byte>;
    using ctrl_block   = detail::ctrl_block<TraitProto, Impl, alloc>;
    using alloc_traits = std::allocator_traits<alloc>;
    auto new_allocator = static_cast<alloc>(allocator);
    const auto [ptr, n] =
        alloc_traits::allocate_at_least(new_allocator, sizeof(ctrl_block) + alignof(ctrl_block));
    const auto destructor_ptr = static_cast<void (*)(void*)>(&ctrl_block::destroy);
    auto       new_n          = n;
    auto       new_ptr        = static_cast<void*>(ptr);
    auto       ctrl_ptr       = std::align(alignof(ctrl_block), sizeof(ctrl_block), new_ptr, new_n);
    if (ctrl_ptr == nullptr) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw std::runtime_error("Could not allocate aligned storage");
    }
    try {
        auto cptr       = new (ctrl_ptr) ctrl_block{.memstart_{ptr},
                                                    .n_{n},
                                                    .destructor_ptr_{destructor_ptr},
                                                    .impl_{std::forward<Args>(args)...},
                                                    .allocator_{std::move(new_allocator)}};
        auto impl_ptr   = &(cptr->impl_);
        auto vtable_ptr = detail::trait_vtable<TraitProto, Impl>::value.data();
        using efptr     = void*;
        return shared_trait<TraitProto>(
            detail::shared_manager(reinterpret_cast<const efptr*>(vtable_ptr), static_cast<void*>(impl_ptr)));
    } catch (...) {
        alloc_traits::deallocate(new_allocator, ptr, n);
        throw;
    }
}

// clang-format off
struct trait_proto {
    void baz(e2);

    void bar00(e0){std::println("bar00(e0)");};
    void bar01(e0){std::println("bar01(e0)");};
    void bar02(e0){std::println("bar02(e0)");};
    void bar03(e0){std::println("bar03(e0)");};
    void bar04(e0){std::println("bar04(e0)");};
    void bar05(e0){std::println("bar05(e0)");};
    void bar06(e0){std::println("bar06(e0)");};
    void bar07(e0){std::println("bar07(e0)");};
    void bar08(e0){std::println("bar08(e0)");};
    void bar09(e0){std::println("bar09(e0)");};
    void bar10(e0){std::println("bar10(e0)");};
    void bar11(e0){std::println("bar11(e0)");};
    void bar12(e0){std::println("bar12(e0)");};
    void bar13(e0){std::println("bar13(e0)");};
    void bar14(e0){std::println("bar14(e0)");};
    void bar15(e0){std::println("bar15(e0)");};
    void bar16(e0){std::println("bar16(e0)");};
    void bar17(e0){std::println("bar17(e0)");};
    void bar18(e0){std::println("bar18(e0)");};
    void bar19(e0){std::println("bar19(e0)");};
    void bar20(e0){std::println("bar20(e0)");};
    void bar21(e0){std::println("bar21(e0)");};
    void bar22(e0){std::println("bar22(e0)");};
    void bar23(e0){std::println("bar23(e0)");};
    void bar24(e0){std::println("bar24(e0)");};
    void bar25(e0){std::println("bar25(e0)");};
    void bar26(e0){std::println("bar26(e0)");};
    void bar27(e0){std::println("bar27(e0)");};
    void bar28(e0){std::println("bar28(e0)");};
    void bar29(e0){std::println("bar29(e0)");};
    void bar30(e0){std::println("bar30(e0)");};
    void bar31(e0){std::println("bar31(e0)");};
    void bar32(e0){std::println("bar32(e0)");};

    void bar(e00){std::println("bar(e00)");};
    void bar(e01){std::println("bar(e01)");};
    void bar(e02){std::println("bar(e02)");};
    void bar(e03){std::println("bar(e03)");};
    void bar(e04){std::println("bar(e04)");};
    void bar(e05){std::println("bar(e05)");};
    void bar(e06){std::println("bar(e06)");};
    void bar(e07){std::println("bar(e07)");};
    void bar(e08){std::println("bar(e08)");};
    void bar(e09){std::println("bar(e09)");};
    void bar(e10){std::println("bar(e10)");};
    void bar(e11){std::println("bar(e11)");};
    void bar(e12){std::println("bar(e12)");};
    void bar(e13){std::println("bar(e13)");};
    void bar(e14){std::println("bar(e14)");};
    void bar(e15){std::println("bar(e15)");};
    // void bar(e16){std::println("bar(e16)");};
    // void bar(e17){std::println("bar(e17)");};
    // void bar(e18){std::println("bar(e18)");};
    // void bar(e19){std::println("bar(e19)");};
    // void bar(e20){std::println("bar(e20)");};
    // void bar(e21){std::println("bar(e21)");};
    // void bar(e22){std::println("bar(e22)");};
    // void bar(e23){std::println("bar(e23)");};
    // void bar(e24){std::println("bar(e24)");};
    // void bar(e25){std::println("bar(e25)");};
    // void bar(e26){std::println("bar(e26)");};
    // void bar(e27){std::println("bar(e27)");};
    // void bar(e28){std::println("bar(e28)");};
    // void bar(e29){std::println("bar(e29)");};
    // void bar(e30){std::println("bar(e30)");};
    // void bar(e31){std::println("bar(e31)");};
    // void bar(e32){std::println("bar(e32)");};

};

consteval {
    define_trait<trait_proto>();
}

// clang-format on
template<typename Trait>
concept any_trait =
    std::meta::is_type(^^Trait)    //
    and
    std::meta::nonstatic_data_members_of(^^Trait, std::meta::access_context::unchecked()).size() == 0    //
    and std::meta::static_data_members_of(^^Trait,
                                          std::meta::access_context::unchecked())
                .size() == 0    //
    and not std::ranges::empty(std::meta::members_of(^^Trait, std::meta::access_context::unchecked()) |
                               std::views::filter(std::not_fn(std::meta::is_special_member_function)))    //
    and std::ranges::empty(std::meta::members_of(^^Trait, std::meta::access_context::unchecked()) |
                           std::views::filter(std::not_fn(std::meta::is_special_member_function)) |
                           std::views::filter(std::meta::is_virtual))    //
    ;

struct my_trait {
    void foo();
};
struct not_trait_data {
    int  v;
    void foo();
};
struct not_trait_stdata {
    static int v;
    void       foo();
};
struct not_trait_empty_fn {};
struct not_trait_virt_fn {
    virtual int foo();
    void        bar();
};

static_assert(any_trait<my_trait>);
static_assert(not any_trait<not_trait_data>);
static_assert(not any_trait<not_trait_stdata>);
static_assert(not any_trait<not_trait_empty_fn>);
static_assert(not any_trait<not_trait_virt_fn>);

int main() {
    auto to =
        make_shared_trait<trait_proto, some_trait_impl>(std::allocator_arg, std::allocator<std::byte>{});
    std::println("sizeof shared trait object {}", sizeof(to));
    std::println("sizeof trait impl {}", sizeof(detail::trait_impl<trait_proto>));
    auto toptr = &to;

    test_16bars(to);
    to = make_shared_trait<trait_proto, other_trait_impl>(std::allocator_arg, std::allocator<std::byte>{});
    test_16bars(to);
}
