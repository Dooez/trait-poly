#pragma once
#include <meta>
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
#include <algorithm>

namespace std {
template<auto V>
struct constant_wrapper {
    using type       = constant_wrapper;
    using value_type = decltype(V);

    static constexpr auto value = V;

    constexpr operator decltype(auto)() const noexcept {
        return value;
    }
};
template<auto V>
constinit auto cw = constant_wrapper<V>{};
}    // namespace std

namespace trp {
namespace detail {
template<typename T>
concept cw_info = meta::has_template_arguments(^^T)                          //
                  and meta::template_of(^^T) == (^^std::constant_wrapper)    //
                  and
[:meta::substitute(^^std::same_as, {^^meta::info, type_of(meta::template_arguments_of (^^T)[0])}):];

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

template<bool Const, bool Volatile, bool Noexcept, typename Ret, typename... Params>
struct wrapper_fptr {
    using obj_ptr = [:[] {
        auto ptr_info = ^^void;
        if (Const)
            ptr_info = meta::add_const(ptr_info);
        if (Volatile)
            ptr_info = meta::add_volatile(ptr_info);
        return meta::add_pointer(ptr_info);
    }():];
    using type    = auto (*)(obj_ptr, Params...) noexcept(Noexcept) -> Ret;
};

template<meta::info Method>
consteval auto wrapper_ptr_for_method() {
    using id    = [:get_method_identity(Method):];
    auto params = std::vector{meta::reflect_constant(id::is_const),    //
                              meta::reflect_constant(id::is_volatile),
                              meta::reflect_constant(id::is_noexcept),
                              ^^id::return_identity::type};
    params.append_range(id::param_identities    //
                        | stdv::transform([]<typename T>(std::type_identity<T>) { return ^^T; }));
    return meta::substitute(^^wrapper_fptr, params);
}

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
    constexpr auto get_impl_method = [](cw_info auto trait_method) {
        constexpr auto make_matcher = [=](info impl_method) {
            auto match_targs =
                vector{^^Impl, reflect_constant(impl_method), get_method_identity(trait_method)};
            match_targs.append_range(parameters_of(trait_method) | stdv::transform(type_of));
            return substitute(^^match_method_strict, match_targs);
        };
        constexpr auto impl_mems = matching_id_public_members<Impl, trait_method>();
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
    auto [... Is] = make_Is<ttt::direct_methods.size()>();
    return vtable<Trait>{
        &([:make_wrapper(ttt::direct_methods[Is], get_impl_method(std::cw<ttt::direct_methods[Is]>)):])...,
        &default_delete<Impl>};
}

template<typename Trait, typename Impl>
struct trait_vtable_for {
    static inline const auto value = fill_vtable<Trait, Impl>();
};

template<any_trait Trait, supertrait_of<Trait> Supertrait>
constexpr void get_explicit_supertrait_vtable_ptr(const vtable<Trait>* ptr) {
    using namespace meta;
    using next_supertrait_t = [:[] {
        if constexpr (direct_supertrait_of<Supertrait, Trait>) {
            return ^^Supertrait;
        } else {
            template for (constexpr auto base: bases_of(^^Trait, ctx_unchecked)) {
                using base_t = [:type_of(base):];
                if constexpr (explicit_supertrait<Supertrait, base>())
                    return type_of(base);
            }
        }
    }():];

    auto next_ptr = [=] {
        template for (constexpr auto mem: members_of(^^vtable<Trait>, ctx_unchecked)) {
            if constexpr (type_of(mem) == ^^vtable<next_supertrait_t>)
                return &(ptr->[:mem:]);
        }
    }();
    if constexpr (^^next_supertrait_t == ^^Supertrait) {
        return next_ptr;
    } else {
        return get_explicit_supertrait_vtable_ptr<next_supertrait_t, Supertrait>(next_ptr);
    }
};
template<non_cvref Impl, meta::info ImplMethod, typename MethodIdentity, typename... Params>
struct invoke_wrapper_struct {
    using return_type    = MethodIdentity::return_type;
    using erased_obj_ptr = [:meta::add_pointer(MethodIdentity::add_obj_cv(^^void)):];
    using obj_ptr        = [:meta::add_pointer(MethodIdentity::add_obj_cv(^^Impl)):];

    static constexpr auto is_noexcept = MethodIdentity::is_noexcept;

    static auto invoke(erased_obj_ptr ptr, Params... params) noexcept(is_noexcept) -> return_type {
        auto&& impl = *static_cast<obj_ptr>(ptr);
        if constexpr (std::meta::is_template(ImplMethod)) {
            return impl.template[:ImplMethod:](std::forward<Params>(params)...);
        } else {
            return impl.[:ImplMethod:](std::forward<Params>(params)...);
        }
    }
};

template<any_trait Trait>
consteval void define_vtable() {
    auto vtable_elements = std::vector<meta::info>{};
    template for (constexpr auto method: trait_traits<Trait>::all_methods) {
        using wrapper_fptr = [:wrapper_ptr_for_method<method>():];
        vtable_elements.push_back(meta::data_member_spec(^^wrapper_fptr::type));
    }
    template for (constexpr auto supertrait: trait_traits<Trait>::direct_supertraits) {
        using supertrait_t = [:type_of(supertrait):];
        define_vtable<supertrait_t>();
        vtable_elements.push_back(meta::data_member_spec(^^vtable<supertrait_t>));
    }
    meta::define_aggregate(^^vtable<Trait>, vtable_elements);
}
template<any_trait Trait, typename Impl>
struct new_trait_vtable_for;

template<any_trait Trait, typename Impl>
consteval auto new_fill_vtable() {
    using namespace std;
    using namespace std::meta;
    using ttt                      = trait_traits<Trait>;
    constexpr auto get_wrapper_ptr = [](auto i) {
        constexpr auto get_impl_method = [](cw_info auto trait_method) {
            constexpr auto make_matcher = [=](info impl_method) {
                auto match_targs =
                    vector{^^Impl, reflect_constant(impl_method), return_type_of(trait_method)};
                match_targs.append_range(parameters_of(trait_method) | stdv::transform(type_of));
                return substitute(^^match_method_strict, match_targs);
            };
            constexpr auto impl_mems = matching_id_public_members<Impl, trait_method>();
            auto [... Is]            = make_Is<impl_mems.size()>();
            auto matched_method      = info{};
            (void)(([:make_matcher(impl_mems[Is]):]() and (matched_method = impl_mems[Is], true)) or ...);
            return matched_method;
        };

        constexpr auto trait_method = std::cw<ttt::all_methods[i]>;
        constexpr auto impl_method  = get_impl_method(trait_method);
        using wrapper_struct        = [:[=] {
            auto wrapper_tparams = vector<info>{};
            wrapper_tparams.push_back(^^Impl);
            wrapper_tparams.push_back(reflect_constant(impl_method));
            wrapper_tparams.push_back(get_method_identity(trait_method));
            wrapper_tparams.append_range(parameters_of(trait_method) | stdv::transform(type_of));
            return substitute(^^invoke_wrapper_struct, wrapper_tparams);
        }():];
        return &wrapper_struct::invoke;
    };
    constexpr auto get_supertrait_vtable = [](auto j) {
        using supertrait_t = [:ttt::direct_supertraits[j]:];
        return new_trait_vtable_for<supertrait_t, Impl>::value;
    };
    // cannot use direct binding since it cannot be declared constexpr
    auto [... Is] = make_Is<ttt::all_methods.size()>();
    auto [... Js] = make_Is<ttt::direct_supertraits.size()>();
    return vtable<Trait>{get_wrapper_ptr(Is)..., get_supertrait_vtable(Js)...};
}
template<any_trait Trait, typename Impl>
struct new_trait_vtable_for {
    static constexpr auto value = new_fill_vtable<Trait, Impl>();
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
    for (auto mem: ttt::direct_methods) {
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
        define_aggregate(type_info,
                         {data_member_spec(invoker_type,
                                           data_member_options{
                                               .name              = name,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
    {
        const auto type_v_info = substitute(^^type_identity, {mgr_info});
        define_aggregate(^^indirect_impl<Trait>,
                         {data_member_spec(type_v_info,
                                           data_member_options{
                                               .name              = "type_v",
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};
}    // namespace trp
