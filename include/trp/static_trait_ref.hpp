#pragma once
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
#include <algorithm>
#include <type_traits>

namespace trp::detail {
template<any_trait Trait, typename Impl>
struct cvts_trait_ref_identity;

template<any_trait Trait, typename Impl, typename... MethodHolders>
class cvts_trait_ref_impl;

// cv-transient static trait reference
template<any_trait Trait, typename Impl>
using cvts_trait_ref = decltype(cvts_trait_ref_identity<Trait, Impl>::type_v)::type;

template<typename T>
concept any_cvts_trait_ref = meta::has_template_arguments(^^T) and meta::template_of(^^T) == ^^cvts_trait_ref;

template<typename Trait, uZ StartIdx>
struct cvts_method_holder;    // holds `mehod_invoker **method_name**;`

template<typename TRef,
         typename MethodHolder,
         typename MethodInvoker,
         typename Impl,
         meta::info,
         any_method_idt>
struct cvts_cvo_invoker;
template<typename TRef,
         typename MethodHolder,
         typename MethodInvoker,
         typename Impl,
         meta::info Method,
         auto       Identifier,
         bool       Const,
         bool       Volatile,
         bool       LVRef,
         bool       RVRef,
         bool       Value,
         bool       Noexcept,
         typename Ret,
         typename... Args>
    requires(not(LVRef or RVRef or Value))    // not supported
struct cvts_cvo_invoker<
    TRef,
    MethodHolder,
    MethodInvoker,
    Impl,
    Method,
    method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>> {
    auto operator()(Args... args) noexcept(Noexcept) -> Ret
        requires(not Const and not Volatile)
    {
        return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
    }
    auto operator()(Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {
        return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
    }
    auto operator()(Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {
        return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
    }
    auto operator()(Args... args) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {
        return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
    }

private:
    auto get_trait_ref(this auto&& self) -> decltype(auto) {
        constexpr auto add_cvp = [](meta::info type) {
            using this_t = std::remove_reference_t<decltype(self)>;
            return meta::add_pointer(copy_cv_to(^^this_t, type));
        };

        static_assert(std::derived_from<MethodInvoker, cvts_cvo_invoker>);
        const auto mi_ptr = static_cast<[:add_cvp(^^MethodInvoker):]>(&self);

        constexpr auto invoker_ptr = [] {
            auto mems = meta::nonstatic_data_members_of(^^MethodHolder, ctx_unchecked);
            if (mems.size() != 1)
                throw "Method holder is expected to have only a single method.";
            if (meta::type_of(mems[0]) != ^^MethodInvoker)
                throw "Method invoker type does not match method holders first member type.";
            return meta::extract<MethodInvoker MethodHolder::*>(mems[0]);
        }();
#ifdef __cpp_lib_is_pointer_interconvertible
        static_assert(std::is_pointer_interconvertible_with_class<MethodHolder>(invoker_ptr));
#endif
        static_assert(std::is_standard_layout_v<MethodHolder>);
        const auto mh_ptr = reinterpret_cast<[:add_cvp(^^MethodHolder):]>(mi_ptr);

        static_assert(std::derived_from<TRef, MethodHolder>);
        return *static_cast<[:add_cvp(^^TRef):]>(mh_ptr);
    }
};
template<typename Impl, meta::info Method, any_method_idt MethodId>
struct cvts_overload_spec {
    using impl_t                      = Impl;
    static constexpr auto impl_method = Method;
    using id                          = MethodId;
};
template<typename TRef, typename MethodHolder, typename... OvSpecs>
struct cvts_cvm_invoker
: cvts_cvo_invoker<TRef,
                   MethodHolder,
                   cvts_cvm_invoker<TRef, MethodHolder, OvSpecs...>,
                   typename OvSpecs::impl_t,
                   OvSpecs::impl_method,
                   typename OvSpecs::id>... {
    using cvts_cvo_invoker<TRef,
                           MethodHolder,
                           cvts_cvm_invoker<TRef, MethodHolder, OvSpecs...>,
                           typename OvSpecs::impl_t,
                           OvSpecs::impl_method,
                           typename OvSpecs::id>::operator()...;
};

template<typename Trait, uZ StartIdx>
struct cvts_cvm_holder;    // holds `mehod_invoker **method_name**;`

template<typename Trait, typename Impl>
consteval void define_cvts_ref() {
    using namespace std;
    using namespace meta;
    if (is_complete_type(substitute(^^cvts_trait_ref_identity, {^^Trait, ^^Impl})))
        return;
    struct method_holder_spec {
        string_view  id;
        vector<info> cvm_invoker_targs;
    };
    template for (constexpr auto supertrait: direct_base_types<Trait>) {
        using supertrait_t = [:copy_cv_to(^^Trait, supertrait):];
        define_cvts_ref<supertrait_t, Impl>();
    }
    auto method_holders_specs    = vector<method_holder_spec>{};
    auto method_holders_specs_c  = vector<method_holder_spec>{};
    auto method_holders_specs_v  = vector<method_holder_spec>{};
    auto method_holders_specs_cv = vector<method_holder_spec>{};
    method_holders_specs.reserve(all_trait_methods<Trait>.size());
    method_holders_specs_c.reserve(all_trait_methods<Trait>.size());
    method_holders_specs_v.reserve(all_trait_methods<Trait>.size());
    method_holders_specs_cv.reserve(all_trait_methods<Trait>.size());

    auto trait_ref_args    = vector<info>{^^Trait, ^^Impl};
    auto trait_ref_args_c  = vector<info>{^^Trait, ^^Impl};
    auto trait_ref_args_v  = vector<info>{^^Trait, ^^Impl};
    auto trait_ref_args_cv = vector<info>{^^Trait, ^^Impl};

    uZ i = 0;
    template for (constexpr auto mem: all_trait_methods<Trait>) {
        using method_idt    = [:mem:];
        constexpr auto spec = substitute(^^cvts_overload_spec,
                                         {^^Impl,    //
                                          reflect_constant(matching_impl_method<Impl, method_idt>),
                                          mem});
        auto add_holder     = [&](auto& method_holders_specs, auto& trait_ref_args, auto trait_info, auto i) {
            auto it = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
            if (it == method_holders_specs.end()) {
                const auto holder_info = substitute(^^cvts_method_holder, {trait_info, reflect_constant(i)});
                method_holders_specs.push_back({
                        .id = method_idt::identifier, .cvm_invoker_targs = {meta::info{}, holder_info, spec}
                });
                trait_ref_args.push_back(holder_info);
            } else {
                it->cvm_invoker_targs.push_back(spec);
            }
        };
        add_holder(method_holders_specs, trait_ref_args, ^^Trait, i);
        if (method_idt::is_const)
            add_holder(method_holders_specs_c, trait_ref_args_c, meta::add_const(^^Trait), i);
        if (method_idt::is_volatile)
            add_holder(method_holders_specs_v, trait_ref_args_v, meta::add_volatile(^^Trait), i);
        if (method_idt::is_const and method_idt::is_volatile)
            add_holder(method_holders_specs_cv, trait_ref_args_cv, meta::add_cv(^^Trait), i);
        ++i;
    }

    constexpr auto define = [](auto& method_holders_specs, auto& trait_ref_args, info trait_inf) {
        const auto ref_info = substitute(^^cvts_trait_ref_impl, trait_ref_args);
        for (auto& [name, cvm_invoker_targs]: method_holders_specs) {
            cvm_invoker_targs[0]    = ref_info;
            const auto invoker_type = substitute(^^cvts_cvm_invoker, cvm_invoker_targs);
            define_aggregate(cvm_invoker_targs[1],
                             {data_member_spec(invoker_type,
                                               data_member_options{
                                                   .name              = name,
                                                   .no_unique_address = true,
                                                   //  .attributes = {^^[[no_unique_address]] },
                                               })});
        }
        {
            const auto type_v_info = substitute(^^type_identity, {ref_info});
            define_aggregate(substitute(^^cvts_trait_ref_identity, {trait_inf, ^^Impl}),
                             {data_member_spec(type_v_info,
                                               data_member_options{
                                                   .name              = "type_v",
                                                   .no_unique_address = true,
                                                   //  .attributes = {^^[[no_unique_address]] },
                                               })});
        }
    };
    define(method_holders_specs, trait_ref_args, ^^Trait);
    define(method_holders_specs_c, trait_ref_args_c, meta::add_const(^^Trait));
    define(method_holders_specs_v, trait_ref_args_v, meta::add_volatile(^^Trait));
    define(method_holders_specs_cv, trait_ref_args_cv, meta::add_cv(^^Trait));
};

template<any_trait Trait, typename Impl, typename... MethodHolders>
class cvts_trait_ref_impl : public MethodHolders... {
    Impl* obj_ptr_{};

    template<typename, typename, typename, typename, meta::info, any_method_idt>
    friend struct cvts_cvo_invoker;

public:
    cvts_trait_ref_impl() = default;
    explicit cvts_trait_ref_impl(Impl& optr)
    : obj_ptr_(&optr) {};
};
}    // namespace trp::detail
