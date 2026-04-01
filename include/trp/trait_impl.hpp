#pragma once
#include <meta>
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
#include <algorithm>


namespace trp {
namespace detail {

template<typename Trait>
    requires any_trait<std::remove_cvref_t<Trait>>
struct indirect_impl;


template<typename Trait>
    requires any_trait<std::remove_cvref_t<Trait>>
using trait_obj = decltype(indirect_impl<Trait>::type_v)::type;

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

    template<typename Manager,
             typename MethodHolder,
             typename MethodInvoker,
             uZ             Index,
             any_method_idt MethodId>
    friend struct overload_invoker;
};

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

template<any_method_idt Method>
constexpr auto wrapper_ptr_for_method = [] {
    using id          = Method;
    using return_type = typename id::return_type;
    auto params       = std::vector{meta::reflect_constant(id::is_const),    //
                              meta::reflect_constant(id::is_volatile),
                              meta::reflect_constant(id::is_noexcept),
                              ^^return_type};
    params.append_range(id::param_infos);
    return meta::substitute(^^wrapper_fptr, params);
}();

template<typename Impl>
void default_delete(void* ptr) {
    delete static_cast<Impl*>(ptr);
}

template<typename Trait, uZ StartIdx>
struct method_holder;    // holds `mehod_invoker **method_name**;`

template<typename Manager, typename MethodHolder, typename MethodInvoker, uZ Index, any_method_idt MethodId>
struct overload_invoker;

template<typename Manager,
         typename MethodHolder,
         typename MethodInvoker,
         uZ   Index,
         auto Identifier,
         bool Const,
         bool Volatile,
         bool LVRef,
         bool RVRef,
         bool Value,
         bool Noexcept,
         typename Ret,
         typename... Args>
    requires(not(LVRef or RVRef or Value))    // not supported
struct overload_invoker<
    Manager,
    MethodHolder,
    MethodInvoker,
    Index,
    method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>> {
    auto operator()(Args... args) noexcept(Noexcept) -> Ret
        requires(not Const and not Volatile)
    {
        return get_method(manager().vtable_ptr_)(manager().obj_ptr_, std::forward<Args>(args)...);
    }
    auto operator()(Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {
        return get_method(manager().vtable_ptr_)(manager().obj_ptr_, std::forward<Args>(args)...);
    }
    auto operator()(Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {
        return get_method(manager().vtable_ptr_)(manager().obj_ptr_, std::forward<Args>(args)...);
    }
    auto operator()(Args... args) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {
        return get_method(manager().vtable_ptr_)(manager().obj_ptr_, std::forward<Args>(args)...);
    }

private:
    template<typename VTable>
    static auto get_method(VTable* vt) {
        constexpr auto m =
            std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<VTable>, ctx_unchecked)[Index];
        return vt->[:m:];
    }
    auto manager(this auto&& self) -> decltype(auto) {
        constexpr auto add_cvp = [](meta::info type) {
            using method_id =
                method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>;
            return meta::add_pointer(method_id::add_obj_cv(type));
        };

        static_assert(std::derived_from<MethodInvoker, overload_invoker>);
        const auto mi_ptr = static_cast<[:add_cvp(^^MethodInvoker):]>(&self);

        static_assert(std::is_standard_layout_v<MethodHolder>);
        // Method invoker is a first member of standard layout type
        const auto mh_ptr = reinterpret_cast<[:add_cvp(^^MethodHolder):]>(mi_ptr);

        static_assert(std::derived_from<Manager, MethodHolder>);
        return *static_cast<[:add_cvp(^^Manager):]>(mh_ptr);
    }
};
template<uZ I, any_method_idt MethodId>
struct overload_spec {
    using id                  = MethodId;
    static constexpr uZ index = I;
};

template<typename Manager, typename MethodHolder, typename... OvSpecs>
struct method_invoker
: overload_invoker<Manager,
                   MethodHolder,
                   method_invoker<Manager, MethodHolder, OvSpecs...>,
                   OvSpecs::index,
                   typename OvSpecs::id>... {
    using overload_invoker<Manager,
                           MethodHolder,
                           method_invoker<Manager, MethodHolder, OvSpecs...>,
                           OvSpecs::index,
                           typename OvSpecs::id>::operator()...;
};

template<meta::info ImplMethod, any_method_idt MethodId, typename... Params>
struct invoke_wrapper_struct {
    using return_type    = MethodId::return_type;
    using erased_obj_ptr = [:meta::add_pointer(MethodId::add_obj_cv(^^void)):];
    using obj_ptr        = [:meta::add_pointer(MethodId::add_obj_cv(meta::parent_of(ImplMethod))):];

    static auto invoke(erased_obj_ptr ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        auto&& impl = *static_cast<obj_ptr>(ptr);
        if constexpr (std::meta::is_template(ImplMethod)) {
            return impl.template[:ImplMethod:](std::forward<Params>(params)...);
        } else {
            return impl.[:ImplMethod:](std::forward<Params>(params)...);
        }
    }
};

template<typename Trait>
struct vtable;

template<any_trait Trait>
consteval void define_vtable() {
    using default_delete_fptr = void (*)(void*);
    auto vtable_elements      = std::vector<meta::info>{};
    template for (constexpr auto method: trait_traits<Trait>::all_methods) {
        using wrapper_fptr_inf = [:[:meta::substitute(^^wrapper_ptr_for_method, {method}):]:];
        vtable_elements.push_back(meta::data_member_spec(^^typename wrapper_fptr_inf::type));
    }
    vtable_elements.push_back(meta::data_member_spec(^^default_delete_fptr, {.name = "default_delete"}));
    template for (constexpr auto supertrait: trait_traits<Trait>::direct_supertraits) {
        using supertrait_t = [:supertrait:];
        define_vtable<supertrait_t>();
        auto supertrait_vtable = meta::substitute(^^vtable, {^^supertrait_t});
        vtable_elements.push_back(meta::data_member_spec(supertrait_vtable));
    }
    meta::define_aggregate(^^vtable<Trait>, vtable_elements);
}

template<any_trait Trait, non_cvref Impl>
struct trait_vtable_for;

template<any_trait Trait, non_cvref Impl>
consteval auto fill_vtable() {
    using namespace std;
    using namespace std::meta;
    using ttt                      = trait_traits<Trait>;
    constexpr auto get_wrapper_ptr = [](cw_info auto trait_method_idt) {
        // constexpr info trait_method_idt    = trait_method_idt_;
        using trait_method_idt_t           = [:trait_method_idt:];
        constexpr auto matched_impl_method = [=] {
            constexpr auto matches = [=](cw_info auto impl_method) {
                return [:substitute(^^strictly_matches, {reflect_constant(impl_method), trait_method_idt}):];
            };
            constexpr auto members = matching_id_public_members<Impl, trait_method_idt_t>();
            template for (constexpr auto m: members) {
                if (matches(std::cw<m>))
                    return m;
            }
            return info{};
        }();

        // use constexpr binding when available
        auto [... trait_is] = make_Is<trait_method_idt_t::param_infos.size()>();

        constexpr auto wrapper_struct_info = [=] {
            return substitute(^^invoke_wrapper_struct,
                              {reflect_constant(matched_impl_method),
                               trait_method_idt,
                               trait_method_idt_t::param_infos[trait_is]...});
        }();
        using wrapper_struct = [:wrapper_struct_info:];
        return &wrapper_struct::invoke;
    };
    constexpr auto get_supertrait_vtable = [](cw_info auto supertriat) {
        using supertrait_t = [:supertriat:];
        return trait_vtable_for<supertrait_t, Impl>::value;
    };
    // use constexpr binding when available
    auto [... Is] = make_Is<ttt::all_methods.size()>();
    auto [... Js] = make_Is<ttt::direct_supertraits.size()>();
    return vtable<Trait>{
        get_wrapper_ptr(std::cw<ttt::all_methods[Is]>)...,
        &default_delete<Impl>,
        get_supertrait_vtable(std::cw<ttt::direct_supertraits[Js]>)...,
    };
}
template<any_trait Trait, non_cvref Impl>
struct trait_vtable_for {
    static constexpr auto value = fill_vtable<Trait, Impl>();
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

}    // namespace detail

template<any_trait Trait>
consteval void define_trait() {
    using namespace std;
    using namespace meta;
    using namespace trp::detail;

    using ttt = trait_traits<Trait>;
    struct method_holder_spec {
        string_view  id;
        vector<info> methods;
        info         type_info{};
    };
    auto method_holder_specs = vector<method_holder_spec>{};
    auto manager_args        = vector<info>{^^vtable<Trait>};

    detail::define_vtable<Trait>();

    uZ i = 0;
    template for (constexpr auto mem: ttt::all_methods) {
        const auto spec  = substitute(^^overload_spec, {reflect_constant(i), mem});
        using method_idt = [:mem:];

        auto it = stdr::find_if(method_holder_specs, [=](auto& p) { return p.id == method_idt::identifier; });
        if (it == method_holder_specs.end()) {
            const auto holder_info = substitute(^^method_holder, {^^Trait, reflect_constant(i)});
            method_holder_specs.push_back(
                {.id = method_idt::identifier, .methods = {spec}, .type_info = holder_info});
            manager_args.push_back(holder_info);
        } else {
            it->methods.push_back(spec);
        }
        ++i;
    }

    const auto mgr_info = substitute(^^trait_impl_manager, manager_args);
    for (auto& [name, overloads, mh_info]: method_holder_specs) {
        auto invoker_args = vector{mgr_info, mh_info};
        invoker_args.append_range(overloads);
        const auto invoker_type = substitute(^^method_invoker, invoker_args);
        define_aggregate(mh_info,
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
