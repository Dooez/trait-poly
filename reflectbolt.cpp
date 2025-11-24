#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
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

enum class e0 {
};
enum class e1 {
};
enum class e2 {
};
enum class e3 {
};

void mock(e0) {
    std::println("e0");
}
void mock(e1) {
    std::println("e1");
}
void mock(e2) {
    std::println("e2");
}
void mock(e3) {
    std::println("e3");
}
// void mock(f32) {
//     std::println("f32");
// }

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

struct shared_manager {
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

template<method_spec... Specs>
struct method_invoker {};
template<uZ Index, typename Ret, typename... Args, method_spec... Specs>
struct method_invoker<method_spec_t<Index, Ret, Args...>, Specs...> : public method_invoker<Specs...> {
    template<typename... CallArgs>
    auto operator()(CallArgs&&... args) {
        using current_t = method_invoker<method_spec_t<Index, Ret, Args...>, Specs...>;
        if constexpr (requires(CallArgs&&... args) { current_t{}.invoke(std::forward<CallArgs>(args)...); }) {
            return invoke(std::forward<CallArgs>(args)...);
        } else {
            return method_invoker<Specs...>::operator()(std::forward<CallArgs>(args)...);
        }
    }
    auto invoke(Args&&... args) const -> Ret {
        auto aligned_ptr     = reinterpret_cast<uZ>(this);
        aligned_ptr          = aligned_ptr - aligned_ptr % sizeof(void*);
        const auto& mngr     = *reinterpret_cast<const detail::shared_manager*>(aligned_ptr);
        using func_t         = auto(void*, Args&&...)->Ret;
        using wrapper_fptr_t = func_t*;
        auto       eptr      = mngr.vtable_begin + Index;
        const auto wrapper   = *reinterpret_cast<const wrapper_fptr_t*>(eptr);
        return wrapper(mngr.obj_ptr, std::forward<Args>(args)...);
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
    constexpr auto n = [] {
        constexpr auto ctx = access_context::current();

        auto trait_members = members_of(^^TraitProto, ctx)                         //
                             | stdv::filter(not_fn(is_special_member_function))    //
                             | stdr::to<vector<info>>();
        return trait_members.size();
    }();
    constexpr auto wrapper_types = [] {
        constexpr auto ctx = access_context::current();

        auto trait_members = members_of(^^TraitProto, ctx)    //
                             | stdv::filter(not_fn(is_special_member_function));
        auto impl_members = members_of(^^Impl, ctx)    //
                            | stdv::filter(not_fn(is_special_member_function));
        auto new_members = vector<info>{};
        for (auto mem: trait_members) {
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
        auto wrappers        = std::array<info, n>{};
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

    auto vtable = []<uZ... Is, auto WTypes>(index_sequence<Is...>, nontype<WTypes>) {
        return array{reinterpret_cast<void*>(&([:WTypes[Is]:]))...};
    }(make_index_sequence<wrapper_types.size()>{}, nontype<wrapper_types>{});
    return vtable;
}

template<typename TraitProto, typename Impl>
struct trait_vtable {
    static inline const auto value = fill_vtable<TraitProto, Impl>();
};
}    // namespace detail

template<typename TraitProto>
consteval void define_trait() {
    using namespace std;
    using namespace std::meta;
    using namespace ::detail;
    // check input trait prototype
    //
    constexpr auto ctx     = access_context::current();
    auto           members = members_of(^^TraitProto, ctx)    //
                   | stdv::filter(not_fn(is_special_member_function));
    auto new_members_raw    = vector<pair<string_view, vector<info>>>{};
    auto new_members        = vector<info>{};
    auto method_spec_params = vector<info>{};
    uZ   i                  = 0;
    for (auto mem: members) {
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
        new_members.push_back(data_member_spec(invoker_type, options));
    }
    define_aggregate(^^trait_impl<TraitProto>, new_members);
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

struct trait_proto {
    void baz(e2);

    void bar(e1);
    void bar(e0);
};
consteval {
    define_trait<trait_proto>();
}
struct some_trait_impl {
    void bar(e0 x) {
        std::println("some impl bar e0");
    }
    void bar(e1 x) {
        std::println("some impl bar e1");
    };
    void baz(e2 x) {
        std::println("some impl baz e2");
    }
};
struct other_trait_impl {
    void bar(e0 x) {
        std::println("other impl bar e0");
    }
    void bar(e1 x) {
        std::println("other impl bar e1");
    };
    void baz(e2 x) {
        std::println("other impl baz e2");
    }
};

void foo(shared_trait<trait_proto> tr) {
    tr.bar(e0{});
};

int main() {
    auto to =
        make_shared_trait<trait_proto, some_trait_impl>(std::allocator_arg, std::allocator<std::byte>{});
    to.bar(e0{});
    to.bar(e1{});
    to = make_shared_trait<trait_proto, other_trait_impl>(std::allocator_arg, std::allocator<std::byte>{});
    to.bar(e0{});
    to.bar(e1{});
}
