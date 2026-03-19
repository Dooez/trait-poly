#pragma once
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif

namespace trp {
namespace detail {
template<auto V>
struct nontype {
    static constexpr auto value = V;
};

template<typename Trait>
struct trait_impl;

union obj_or_ptr_t {
    void*                              ptr;
    std::array<std::byte, sizeof(ptr)> storage;
};

struct alignas(sizeof(void*) * 2) trait_impl_manager {
    using efptr = void*;
    const efptr* vtable_begin{};
    void*        obj_ptr{};
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
    auto operator()(Args&&... args) const -> Ret {
        const auto& mngr       = *reinterpret_cast<const trait_impl_manager*>(this);
        using func_t           = auto(void*, Args&&...)->Ret;
        using wrapper_fptr_t   = func_t*;
        const auto erased_ptr  = mngr.vtable_begin + Index;
        const auto wrapper_ptr = *reinterpret_cast<const wrapper_fptr_t*>(erased_ptr);
        return wrapper_ptr(mngr.obj_ptr, std::forward<Args>(args)...);
    }
};
template<method_spec... Specs>
struct method_invoker : overload_invoker<Specs>... {
    using overload_invoker<Specs>::operator()...;
};

template<typename Impl, std::meta::info Method, typename Ret, typename... Args>
auto invoke_wrapper(void* impl_ptr, Args&&... args) -> Ret {
    auto& impl = *static_cast<Impl*>(impl_ptr);
    if constexpr (std::meta::is_template(Method)) {
        return impl.template[:Method:](std::forward<Args>(args)...);
    } else {
        return impl.[:Method:](std::forward<Args>(args)...);
    }
}

template<typename Trait, typename Impl>
auto fill_vtable() {
    using namespace std;
    using namespace std::meta;
    using ttt                      = trait_traits<Trait>;
    constexpr auto get_impl_method = []<info TraitMethod>(nontype<TraitMethod>) {
        constexpr auto make_matcher = [](info impl_method) {
            auto match_targs = vector{^^Impl, reflect_constant(impl_method), return_type_of(TraitMethod)};
            match_targs.append_range(parameters_of(TraitMethod) | stdv::transform(type_of));
            return substitute(^^match_method, match_targs);
        };
        constexpr auto impl_mems = matching_id_methods<Impl, TraitMethod>();
        auto [... Is]            = make_Is<impl_mems.size()>();
        auto matched_method      = info{};
        (void)(([:make_matcher(impl_mems[Is]):]() && (matched_method = impl_mems[Is], true)) || ...);
        return matched_method;
    };
    constexpr auto make_wrapper = [](info trait_method, info impl_method) {
        auto wrapper_tparams = vector<info>{};
        wrapper_tparams.push_back(^^Impl);
        wrapper_tparams.push_back(reflect_constant(impl_method));
        wrapper_tparams.push_back(return_type_of(trait_method));
        wrapper_tparams.append_range(parameters_of(trait_method) | stdv::transform(type_of));
        return substitute(^^invoke_wrapper, wrapper_tparams);
    };
    auto [... Is] = make_Is<ttt::methods.size()>();
    return array{reinterpret_cast<void*>(
        &([:make_wrapper(ttt::methods[Is], get_impl_method(nontype<ttt::methods[Is]>())):]))...};
}
template<typename Trait, implements_trait<Trait> Impl>
struct trait_vtable {
    static inline const auto value = fill_vtable<Trait, Impl>();
};

template<typename TraitImpl>
consteval auto validate_method_offsets() {
    using namespace std::meta;
    return stdr::empty(nonstatic_data_members_of(^^TraitImpl, ctx_unchecked) |
                       stdv::filter([](auto info) { return offset_of(info).bytes != 0; }));
}
}    // namespace detail

template<typename Trait>
consteval void define_trait() {
    using namespace std;
    using namespace std::meta;
    using namespace trp::detail;

    using ttt               = trait_traits<Trait>;
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
    define_aggregate(^^trait_impl<Trait>, methods);
};
}    // namespace trp
