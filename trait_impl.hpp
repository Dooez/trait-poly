#pragma once
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
#include <algorithm>

namespace trp {
namespace detail {
template<auto V>
struct nontype {
    static constexpr auto value = V;
};

template<typename Trait>
struct indirect_impl;

template<typename Trait>
using trait_impl = decltype(indirect_impl<Trait>::type_v)::type;

template<uZ Index, typename Ret, typename... Args>
struct method_spec_t {};
template<typename T>
concept overload_spec =
    std::meta::has_template_arguments(^^T) and std::meta::template_of(^^T) == ^^method_spec_t;

template<typename Manager, typename MethodHolder, typename MethodInvoker, overload_spec Spec>
struct overload_invoker;

template<typename VTable, typename... MethodHolders>
struct trait_impl_manager : public MethodHolders... {
protected:
    using vtable_t = VTable;

    trait_impl_manager() = default;
    trait_impl_manager(const vtable_t* vptr, void* optr)
    : vtable_ptr_(vptr)
    , obj_ptr_(optr) {};

    const vtable_t* vtable_ptr_{};
    void*           obj_ptr_{};

    template<typename Manager, typename MethodHolder, typename MethodInvoker, overload_spec Spec>
    friend struct overload_invoker;
};

template<typename Trait>
struct vtable;

template<overload_spec Spec>
struct fptr;
template<uZ Index, typename Ret, typename... Args>
struct fptr<method_spec_t<Index, Ret, Args...>> {
    using type = auto (*)(void*, Args...) -> Ret;
};
template<overload_spec Spec>
using fptr_t = fptr<Spec>::type;

template<typename Impl>
void default_delete(void* ptr) {
    delete static_cast<Impl*>(ptr);
}

template<uZ I, typename VTable>
auto& get_method(const VTable* vt) {
    constexpr auto m = std::meta::nonstatic_data_members_of(^^VTable, ctx_unchecked)[I];
    return vt->[:m:];
}

template<typename Manager,
         typename MethodHolder,
         typename MethodInvoker,
         uZ Index,
         typename Ret,
         typename... Args>
struct overload_invoker<Manager, MethodHolder, MethodInvoker, method_spec_t<Index, Ret, Args...>> {
    auto operator()(Args... args) const -> Ret {
        static_assert(std::derived_from<MethodInvoker, overload_invoker>);
        const auto* mi_ptr = static_cast<const MethodInvoker*>(this);

        static_assert(std::is_standard_layout_v<MethodHolder>);
        // Method invoker is a first member of standard layout type
        const auto* mh_ptr = reinterpret_cast<const MethodHolder*>(mi_ptr);

        static_assert(std::derived_from<Manager, MethodHolder>);
        const auto& mngr = *static_cast<const Manager*>(mh_ptr);

        return get_method<Index>(mngr.vtable_ptr_)(mngr.obj_ptr_, std::forward<Args>(args)...);
    }
};

template<typename Manager, typename MethodHolder, overload_spec... Specs>
struct method_invoker
: overload_invoker<Manager, MethodHolder, method_invoker<Manager, MethodHolder, Specs...>, Specs>... {
    using overload_invoker<Manager, MethodHolder, method_invoker<Manager, MethodHolder, Specs...>, Specs>::
    operator()...;
};

template<typename Trait, uZ StartIdx>
struct method_holder;    // holds `mehod_invoker **method_name**;`

template<typename Impl, std::meta::info Method, typename Ret, typename... Args>
auto invoke_wrapper(void* impl_ptr, Args... args) -> Ret {
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
            return substitute(^^match_method_strict, match_targs);
        };
        constexpr auto impl_mems = matching_id_methods<Impl, TraitMethod>();
        auto [... Is]            = make_Is<impl_mems.size()>();
        auto matched_method      = info{};
        (void)(([:make_matcher(impl_mems[Is]):]() and (matched_method = impl_mems[Is], true)) or ...);
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
    return vtable<Trait>{
        &([:make_wrapper(ttt::methods[Is], get_impl_method(nontype<ttt::methods[Is]>())):])...,
        &default_delete<Impl>};
}

template<typename Trait, typename Impl>
struct trait_vtable_for {
    static inline const auto value = fill_vtable<Trait, Impl>();
};
}    // namespace detail

template<any_trait Trait>
consteval void define_trait() {
    using namespace std;
    using namespace std::meta;
    using namespace trp::detail;

    using ttt = trait_traits<Trait>;
    struct method_holder_spec {
        string_view  id;
        vector<info> methods;
        info         type_info{};
    };
    auto method_holder_specs = vector<method_holder_spec>{};
    auto manager_args        = vector<info>{^^vtable<Trait>};
    auto vtable_members      = vector<info>{};

    uZ i = 0;
    for (auto mem: ttt::methods) {
        auto overload_spec_args = vector<info>{};
        overload_spec_args.push_back(reflect_constant(i));
        overload_spec_args.push_back(return_type_of(mem));
        overload_spec_args.append_range(parameters_of(mem) | stdv::transform(type_of));
        const auto spec = substitute(^^method_spec_t, overload_spec_args);

        vtable_members.push_back(data_member_spec(substitute(^^fptr_t, {spec}), {}));

        auto it = stdr::find_if(method_holder_specs, [=](auto& p) { return p.id == identifier_of(mem); });
        if (it == method_holder_specs.end()) {
            const auto holder_info = substitute(^^method_holder, {^^Trait, reflect_constant(i)});
            method_holder_specs.push_back(
                {.id = identifier_of(mem), .methods = {spec}, .type_info = holder_info});
            manager_args.push_back(holder_info);
        } else {
            it->methods.push_back(spec);
        }
        ++i;
    }
    using default_delete_t = auto (*)(void*)->void;
    vtable_members.push_back(data_member_spec(^^default_delete_t, {.name = "default_delete"}));
    define_aggregate(^^vtable<Trait>, vtable_members);
    const auto mgr_info = substitute(^^trait_impl_manager, manager_args);
    for (auto& [name, overloads, type_info]: method_holder_specs) {
        auto invoker_args = vector{mgr_info, type_info};
        invoker_args.append_range(overloads);
        const auto invoker_type = substitute(^^method_invoker, invoker_args);
        define_aggregate(
            type_info,
            {data_member_spec(invoker_type, {.name = name, .attributes = {^^[[no_unique_address]]}})});
    }
    {
        const auto type_v_info = substitute(^^type_identity, {mgr_info});
        define_aggregate(^^indirect_impl<Trait>,
                         {data_member_spec(type_v_info,
                                           data_member_options{.name       = "type_v",
                                                               .attributes = {^^[[no_unique_address]]}})});
    }
};
}    // namespace trp
