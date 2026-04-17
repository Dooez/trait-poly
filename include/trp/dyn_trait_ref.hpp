#pragma once
#include <meta>
#ifndef TRP_GODBOLT
#include "static_trait_ref.hpp"
#include "trp_concepts.hpp"
#endif
#include <algorithm>
#include <type_traits>

namespace trp {
namespace detail {
template<any_trait Trait, typename... MethodHolders>
class dyn_trait_ref_impl;

template<any_trait Trait>
struct dyn_trait_ref_identity;

template<typename T>
struct empty_default_implementation {};

template<any_trait T>
struct trait_impl_mock : public T {};

template<typename ParamType>
consteval bool first_parameter_is_cvref_of(meta::info fn) {
    auto params = meta::parameters_of(fn);
    return stdr::size(params) > 0                                   //
           and meta::is_reference_type(meta::type_of(params[0]))    //
           and meta::remove_cvref(meta::type_of(params[0])) == ^^ParamType;
}
consteval auto default_impl_to_method_identity(meta::info fn) {
    using namespace meta;
    auto params    = parameters_of(fn);
    auto impl      = remove_reference(type_of(params[0]));
    auto idt_targs = std::vector{reflect_constant_string(identifier_of(fn)),
                                 reflect_constant(is_const(impl)),
                                 reflect_constant(is_volatile(impl)),
                                 reflect_constant(false),
                                 reflect_constant(false),
                                 reflect_constant(false),
                                 reflect_constant(is_noexcept(fn)),
                                 return_type_of(fn)};
    idt_targs.append_range(params | stdv::drop(1));
    return substitute(^^method_identity_t, idt_targs);
}

template<any_trait Trait>
consteval bool maps_to_a_trait_method_of(meta::info fn) {
    return stdr::contains(all_trait_methods<Trait>, default_impl_to_method_identity(fn));
};

template<template<typename> typename DefaultImpl, typename Trait>
concept default_impl_for =
    any_trait<Trait>                                                                                //
    and stdr::all_of(nonspecial_members<DefaultImpl<trait_impl_mock<Trait>>>,                       //
                     meta::is_static_member)                                                        //
    and stdr::all_of(nonspecial_members<DefaultImpl<trait_impl_mock<Trait>>>, meta::is_function)    //
    and stdr::all_of(nonspecial_members<DefaultImpl<trait_impl_mock<Trait>>>, meta::is_function)    //
    and stdr::all_of(nonspecial_members<DefaultImpl<trait_impl_mock<Trait>>>,
                     first_parameter_is_cvref_of<trait_impl_mock<Trait>>)    //
    ;
template<template<typename> typename DefaultImpl, typename Trait>
concept strict_default_impl_for =
    default_impl_for<DefaultImpl, Trait>    //
    and
    stdr::all_of(nonspecial_members<DefaultImpl<trait_impl_mock<Trait>>>, maps_to_a_trait_method_of<Trait>);

template<non_cv_trait Trait>
struct default_trait_impl_identity;

template<any_trait Trait>
inline constexpr auto mandatory_trait_methods = [] {
    static constexpr meta::info default_impl_template =
        decltype(default_trait_impl_identity<std::remove_cv_t<Trait>>::template_info)::value;
    using default_trait_impl_mock = [:meta::substitute(default_impl_template, {^^Trait}):];
    auto methods                  = std::vector(std::from_range, all_trait_methods<Trait>);
    for (auto m: nonspecial_members<default_trait_impl_mock>    //
                     | stdv::transform(default_impl_to_method_identity)) {
        auto [s, e] = stdr::remove(methods, m);
        methods.erase(s, e);
    }
    return std::define_static_array(methods);
}();


template<meta::info Self, typename Impl, typename Trait, uZ I = 0>
concept implements_methods =
    requires { requires I == stdr::size(mandatory_trait_methods<Trait>); }                        //
    or ([:meta::substitute(^^implements_method, {^^Impl, mandatory_trait_methods<Trait>[I]}):]    //
        and
           [:meta::substitute(
                 Self, {meta::reflect_constant(Self), ^^Impl, ^^Trait, meta::reflect_constant(I + 1)}):]);

}    // namespace detail

template<typename Impl, typename Trait>
concept implements_trait = any_trait<Trait> and std::is_class_v<Impl> and
                           detail::implements_methods<^^detail::implements_methods, Impl, Trait>;
template<any_trait Trait>
struct dyn_trait_ref;

template<typename Supertrait, any_trait Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
[[nodiscard]] auto trait_cast(const dyn_trait_ref<Trait>& ref) -> dyn_trait_ref<Supertrait> {
    return ref;
}
template<typename Impl, any_trait Trait>
    requires implements_trait<Impl, Trait>
[[nodiscard]] auto is_holding_type(const dyn_trait_ref<Trait>& ref) -> bool {
    return is_holding_type<Impl>(ref);
}
namespace detail {

template<non_ref T>
struct unique_id_struct {
    static inline char value{};
};

template<typename T>
concept any_dyn_trait_ref = meta::has_template_arguments(^^T) and meta::template_of(^^T) == ^^dyn_trait_ref;

template<any_method_idt Method>
using wrapper_fptr_for = Method::wrapper_fptr_type;

template<typename Impl>
void default_delete(void* ptr) {
    delete static_cast<Impl*>(ptr);
}

template<typename Trait, uZ StartIdx>
struct method_holder;    // holds `mehod_invoker **method_name**;`

template<non_cv_trait Trait>
struct vtable;

template<uZ Index, any_method_idt MethodId>
struct cvo_invoker;
template<uZ   Index,
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
struct cvo_invoker<
    Index,
    method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>> {
    auto operator()(const auto* vtable_ptr, void* obj_ptr, Args... args) noexcept(Noexcept) -> Ret
        requires(not Const and not Volatile)
    {
        return get_method(vtable_ptr)(obj_ptr, std::forward<Args>(args)...);
    }
    auto operator()(const auto* vtable_ptr, void* obj_ptr, Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {
        return get_method(vtable_ptr)(obj_ptr, std::forward<Args>(args)...);
    }
    auto operator()(const auto* vtable_ptr, void* obj_ptr, Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {
        return get_method(vtable_ptr)(obj_ptr, std::forward<Args>(args)...);
    }
    auto operator()(const auto* vtable_ptr, void* obj_ptr, Args... args) const volatile noexcept(Noexcept)
        -> Ret
        requires(Const and Volatile)
    {
        return get_method(vtable_ptr)(obj_ptr, std::forward<Args>(args)...);
    }

private:
    template<typename VTable>
    static auto get_method(VTable* vt) {
        constexpr auto m = std::meta::nonstatic_data_members_of(^^VTable, ctx_unchecked)[Index];
        return vt->[:m:];
    }
};
template<uZ I, any_method_idt MethodId>
struct overload_spec {
    using id                  = MethodId;
    static constexpr uZ index = I;
};
template<typename... OvSpecs>
struct cvm_invoker : cvo_invoker<OvSpecs::index, typename OvSpecs::id>... {
    using cvo_invoker<OvSpecs::index, typename OvSpecs::id>::operator()...;
};

template<typename Trait, uZ StartIdx>
struct cvm_holder;    // holds `mehod_invoker **method_name**;`

template<typename CVMInvoker, typename VTable, typename... Args>
concept noexcept_cvm_invoker =
    meta::is_template(^^CVMInvoker) and (meta::template_of(^^CVMInvoker) == ^^cvm_invoker) and ([] {
        CVMInvoker invoker{};
        VTable     vt{};
        return noexcept(invoker(&vt, nullptr, std::forward<Args>(std::declval<Args>())...));
    });


template<typename TRef, typename MethodHolder, typename CVMInvoker>
struct method_invoker {
private:
    using trait_t  = [:meta::template_arguments_of(^^TRef)[0]:];
    using vtable_t = vtable<std::remove_cv_t<trait_t>>;

public:
    template<typename... Args>
    auto operator()(Args&&... args) const
        volatile noexcept(noexcept_cvm_invoker<CVMInvoker, vtable_t, Args...>) -> decltype(auto) {
        return CVMInvoker{}(
            get_trait_ref().vtable_ptr_, get_trait_ref().obj_ptr_, std::forward<Args>(args)...);
    }

private:
    auto get_trait_ref(this auto&& self) -> decltype(auto) {
        constexpr auto add_cvp = [](meta::info type) {
            using this_t = std::remove_reference_t<decltype(self)>;
            return meta::add_pointer(copy_cv_to(^^this_t, type));
        };

        constexpr auto invoker_ptr = [] {
            auto mems = meta::nonstatic_data_members_of(^^MethodHolder, ctx_unchecked);
            if (mems.size() != 1)
                throw "Method holder is expected to have only a single method.";
            if (meta::type_of(mems[0]) != ^^method_invoker)
                throw "Method invoker type does not match method holders first member type.";
            return meta::extract<method_invoker MethodHolder::*>(mems[0]);
        }();
#ifdef __cpp_lib_is_pointer_interconvertible
        static_assert(std::is_pointer_interconvertible_with_class<MethodHolder>(invoker_ptr));
#endif
        static_assert(std::is_standard_layout_v<MethodHolder>);
        const auto mh_ptr = reinterpret_cast<[:add_cvp(^^MethodHolder):]>(&self);

        static_assert(std::derived_from<TRef, MethodHolder>);
        return *static_cast<[:add_cvp(^^TRef):]>(mh_ptr);
    }
};

template<any_trait Trait, non_ref Impl, any_method_idt MethodId, typename... Params>
struct defaul_invoke_wrapper_struct {
    using return_type = MethodId::return_type;
    using ref_t       = [:MethodId::add_obj_cv(^^cvts_trait_ref<Trait, Impl>):];

    static constexpr meta::info default_impl_template =
        default_trait_impl_identity<Trait>::template_info::value;

    using default_trait_impl = [:meta::substitute(default_impl_template, {^^Impl}):];

    static constexpr auto default_method =
        *stdr::find(nonspecial_members<default_trait_impl>, ^^MethodId, default_impl_to_method_identity);

    static auto invoke(void* ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        ref_t ref(*static_cast<Impl*>(ptr));
        return [:default_method:](ref, std::forward<Params>(params)...);
    }
};
template<non_ref Impl, meta::info ImplMethod, any_method_idt MethodId, typename... Params>
struct invoke_wrapper_struct {
    using return_type = MethodId::return_type;
    using obj_ptr     = [:meta::add_pointer(MethodId::add_obj_cv(^^Impl)):];

    static auto invoke(void* ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        auto&& impl = *static_cast<obj_ptr>(ptr);
        if constexpr (std::meta::is_template(ImplMethod)) {
            return impl.template[:ImplMethod:](std::forward<Params>(params)...);
        } else {
            return impl.[:ImplMethod:](std::forward<Params>(params)...);
        }
    }
};

struct vtable_cv_quals {
    bool has_full     = true;
    bool has_const    = true;
    bool has_volatile = true;
    // bool has_cv=true; not needed since this is a biggest supertrait
};

template<non_cv_trait Trait>
consteval void define_vtable() {
    using namespace meta;
    if (is_complete_type(^^vtable<Trait>))
        return;

    constexpr auto get_info_to_member = []<uZ N>(const char (&prefix)[N], auto type_getter) {
        auto res = std::array<char, N + 20>{};
        stdr::copy(prefix, res.begin());
        auto i = 0UZ;
        return [=](auto info) mutable {
            auto id_end = std::to_chars(&*(res.begin() + N - 1), &*res.end(), i++);
            if (id_end.ec != std::errc{})
                throw "Error while forming member name";
            auto id = std::string_view(res.data(), id_end.ptr);
            return meta::data_member_spec(type_getter(info), {.name = id});
        };
    };

    auto vtable_elements = std::vector<info>{};
    auto method_spec = get_info_to_member("m_", [](info m) { return substitute(^^wrapper_fptr_for, {m}); });
    for (auto method_idt: all_trait_methods<Trait>) {
        vtable_elements.push_back(method_spec(method_idt));
    }
    using default_delete_fptr = void (*)(void*);
    vtable_elements.push_back(data_member_spec(^^default_delete_fptr, {.name = "default_delete"}));
    using id_ptr = const char*;
    vtable_elements.push_back(data_member_spec(^^id_ptr, {.name = "id_ptr"}));
    vtable_elements.push_back(data_member_spec(^^vtable_cv_quals, {.name = "cv_quals"}));

    auto supertrait_spec = get_info_to_member(
        "supertrait_", [](info s) { return substitute(^^vtable, {copy_cv_to(^^Trait, s)}); });
    for (auto supertrait: direct_base_types<Trait>) {
        // not defining supertrait vtable because maybe_define_cv_trait calls define_vtable for each trait in the hierarchy
        vtable_elements.push_back(supertrait_spec(supertrait));
    }
    define_aggregate(^^vtable<Trait>, vtable_elements);
}

template<non_cv_trait Trait, non_ref Impl>
consteval auto fill_vtable();

template<non_cv_trait Trait, non_ref Impl>
inline constexpr auto trait_vtable_for = fill_vtable<Trait, Impl>();

template<non_cv_trait Trait, non_ref Impl>
consteval auto fill_vtable() {
    using namespace meta;
    auto       quals           = vtable_cv_quals{};
    const auto get_wrapper_ptr = [&](cw_info auto trait_method_idt) {
        using trait_method_idt_t = [:trait_method_idt:];
        // Common vtable is used for any combination of cv-qualification of trait.
        // Implementation is checked by externally.
        // Fill the missing vtable elements with nullptr.
        if constexpr (matching_impl_method<Impl, trait_method_idt_t> == info{}) {
            if (trait_method_idt_t::is_const)
                quals.has_const = false;
            if (trait_method_idt_t::is_volatile)
                quals.has_volatile = false;
            quals.has_full = false;
            return typename trait_method_idt_t::wrapper_fptr_type{nullptr};
        } else {
            constexpr auto wrapper_struct_info = [=] {
                auto [... infos] = trait_method_idt_t::param_infos;
                return substitute(^^invoke_wrapper_struct,
                                  {^^Impl,
                                   reflect_constant(matching_impl_method<Impl, trait_method_idt_t>),
                                   trait_method_idt,
                                   infos...});
            }();
            using wrapper_struct = [:wrapper_struct_info:];
            return &wrapper_struct::invoke;
        }
    };

    // use constexpr binding when it becomes available
    auto [... Is] = make_cw_idxs<all_trait_methods<Trait>.size()>();
    auto [... Js] = make_cw_idxs<direct_base_types<Trait>.size()>();
    return vtable<Trait>{
        get_wrapper_ptr(cw<all_trait_methods<Trait>[Is]>)...,
        &default_delete<Impl>,
        &unique_id_struct<Impl>::value,
        quals,
        [:meta::substitute(^^trait_vtable_for,
                           {copy_cv_to(^^Trait, direct_base_types<Trait>[Js]), ^^Impl}):]...,
    };
}

template<non_cv_trait Supertrait, non_cv_trait Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
constexpr auto get_explicit_supertrait_vtable_ptr(const vtable<Trait>* ptr) -> const vtable<Supertrait>* {
    if constexpr (std::same_as<Trait, Supertrait>)
        return ptr;

    using namespace meta;
    using next_supertrait_t = [:[] -> meta::info {
        if constexpr (direct_supertrait_of<Supertrait, Trait>) {
            return ^^Supertrait;
        } else {
            for (auto base: direct_base_types<Trait>) {
                auto is_derived = meta::substitute(^^std::derived_from, {base, ^^Supertrait});
                if (meta::extract<bool>(is_derived))
                    return base;
            }
            std::unreachable();
        }
    }():];

    const auto next_ptr = [=] -> const vtable<next_supertrait_t>* {
        static constexpr auto mems = std::define_static_array(
            meta::nonstatic_data_members_of(^^vtable<Trait>, meta::access_context::unprivileged()) |
            stdv::drop(stdr::size(all_trait_methods<Trait>)    //
                       + 1                                     // default_deleter
                       + 1                                     // id_ptr
                       + 1                                     // cv_quals
                       ));
        template for (constexpr auto m: mems) {
            if constexpr (type_of(m) == substitute(^^vtable, {^^next_supertrait_t}))
                return &((*ptr).[:m:]);
        }
        std::unreachable();
    }();
    if constexpr (std::same_as<next_supertrait_t, Supertrait>) {
        return next_ptr;
    } else {
        return get_explicit_supertrait_vtable_ptr<Supertrait>(next_ptr);
    }
};

template<any_trait Trait, typename... MethodHolders>
void ref_release(dyn_trait_ref_impl<Trait, MethodHolders...>& ref) {
    ref.obj_ptr_ = nullptr;
}
template<any_trait Trait, typename... MethodHolders>
void ref_rebind(dyn_trait_ref_impl<Trait, MethodHolders...>&       ref,
                const dyn_trait_ref_impl<Trait, MethodHolders...>& other) {
    ref.vtable_ptr_ = other.vtable_ptr_;
    ref.obj_ptr_    = other.obj_ptr_;
}
template<any_trait Trait, typename... MethodHolders>
[[nodiscard]] auto ref_holds_value(const dyn_trait_ref_impl<Trait, MethodHolders...>& ref) -> bool {
    return ref.obj_ptr_ != nullptr;
}
template<any_trait Trait, typename... MethodHolders>
void ref_default_delete(const dyn_trait_ref_impl<Trait, MethodHolders...>& ref) {
    ref.vtable_ptr_->default_delete(ref.obj_ptr_);
}
template<any_trait Trait, typename... MethodHolders>
class dyn_trait_ref_impl : public MethodHolders... {
    const vtable<std::remove_cv_t<Trait>>* vtable_ptr_{};
    void*                                  obj_ptr_{};

    dyn_trait_ref_impl() = default;
    dyn_trait_ref_impl(const vtable<std::remove_cv_t<Trait>>* vptr, auto* optr)
    : vtable_ptr_(vptr)
    , obj_ptr_(
          (void*)optr)    // NOLINT(*cast*)cv qualification is kept via `invoke_wrapper_struct<...>::obj_ptr`
    {};

    template<any_trait U, typename... MHs>
    friend void ref_release(dyn_trait_ref_impl<U, MHs...>& ref);
    template<any_trait U, typename... MHs>
    friend void ref_rebind(dyn_trait_ref_impl<U, MHs...>& ref, const dyn_trait_ref_impl<U, MHs...>& other);
    template<any_trait U, typename... MHs>
    friend auto ref_holds_value(const dyn_trait_ref_impl<U, MHs...>& ref) -> bool;
    template<any_trait U, typename... MHs>
    friend void ref_default_delete(const dyn_trait_ref_impl<U, MHs...>& ref);

    template<typename, typename, typename>
    friend struct method_invoker;

    template<any_trait>
    friend class ::trp::dyn_trait_ref;
};

template<non_cv_trait Trait, template<typename> typename DefaultImpl>
    requires default_impl_for<DefaultImpl, Trait>
consteval auto maybe_define_cv_trait() {
    using namespace std;
    using namespace meta;
    if (meta::is_complete_type(^^default_trait_impl_identity<std::remove_cv_t<Trait>>))
        return;
    define_aggregate(^^default_trait_impl_identity<Trait>,
                     {data_member_spec(substitute(^^constant_wrapper, {reflect_constant(^^DefaultImpl)}),
                                       {.name = "template_info"})});

    struct method_holder_spec {
        string_view  id;
        info         holder_info{};
        vector<info> cvm_invoker_targs;
    };
    template for (constexpr auto supertrait: direct_base_types<Trait>) {
        using supertrait_t = [:copy_cv_to(^^Trait, supertrait):];
        maybe_define_cv_trait<supertrait_t, DefaultImpl>();
    }
    define_vtable<Trait>();
    auto method_holders_specs    = vector<method_holder_spec>{};
    auto method_holders_specs_c  = vector<method_holder_spec>{};
    auto method_holders_specs_v  = vector<method_holder_spec>{};
    auto method_holders_specs_cv = vector<method_holder_spec>{};
    method_holders_specs.reserve(all_trait_methods<Trait>.size());
    method_holders_specs_c.reserve(all_trait_methods<Trait>.size());
    method_holders_specs_v.reserve(all_trait_methods<Trait>.size());
    method_holders_specs_cv.reserve(all_trait_methods<Trait>.size());

    auto dyn_trait_ref_args    = vector<info>{^^Trait};
    auto dyn_trait_ref_args_c  = vector<info>{^^Trait};
    auto dyn_trait_ref_args_v  = vector<info>{^^Trait};
    auto dyn_trait_ref_args_cv = vector<info>{^^Trait};

    uZ i = 0;
    template for (constexpr auto mem: all_trait_methods<Trait>) {
        using method_idt = [:mem:];
        const auto spec  = substitute(^^overload_spec, {reflect_constant(i), mem});
        auto add_holder  = [=](auto& method_holders_specs, auto& trait_ref_args, auto trait_info, auto i) {
            auto it = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
            if (it == method_holders_specs.end()) {
                const auto holder_info = substitute(^^method_holder, {trait_info, reflect_constant(i)});
                method_holders_specs.push_back(
                    {.id = method_idt::identifier, .holder_info = holder_info, .cvm_invoker_targs = {spec}});
                trait_ref_args.push_back(holder_info);
            } else {
                it->cvm_invoker_targs.push_back(spec);
            }
        };
        add_holder(method_holders_specs, dyn_trait_ref_args, ^^Trait, i);
        if (method_idt::is_const)
            add_holder(method_holders_specs_c, dyn_trait_ref_args_c, meta::add_const(^^Trait), i);
        if (method_idt::is_volatile)
            add_holder(method_holders_specs_v, dyn_trait_ref_args_v, meta::add_volatile(^^Trait), i);
        if (method_idt::is_const and method_idt::is_volatile)
            add_holder(method_holders_specs_cv, dyn_trait_ref_args_cv, meta::add_cv(^^Trait), i);
        ++i;
    }

    constexpr auto define = [](auto& method_holders_specs, auto& trait_ref_args, info trait_inf) {
        const auto ref_info = substitute(^^dyn_trait_ref_impl, trait_ref_args);
        for (auto& [name, holder_info, cvm_invoker_targs]: method_holders_specs) {
            const auto cvm_invoker_info = substitute(^^cvm_invoker, cvm_invoker_targs);
            const auto invoker_type = substitute(^^method_invoker, {ref_info, holder_info, cvm_invoker_info});
            define_aggregate(holder_info,
                             {data_member_spec(invoker_type,
                                               data_member_options{
                                                   .name              = name,
                                                   .no_unique_address = true,
                                                   //  .attributes = {^^[[no_unique_address]] },
                                               })});
        }
        {
            const auto type_v_info = substitute(^^type_identity, {ref_info});
            define_aggregate(substitute(^^dyn_trait_ref_identity, {trait_inf}),
                             {data_member_spec(type_v_info,
                                               data_member_options{
                                                   .name              = "type_v",
                                                   .no_unique_address = true,
                                                   //  .attributes = {^^[[no_unique_address]] },
                                               })});
        }
    };
    define(method_holders_specs, dyn_trait_ref_args, ^^Trait);
    define(method_holders_specs_c, dyn_trait_ref_args_c, meta::add_const(^^Trait));
    define(method_holders_specs_v, dyn_trait_ref_args_v, meta::add_volatile(^^Trait));
    define(method_holders_specs_cv, dyn_trait_ref_args_cv, meta::add_cv(^^Trait));
};
}    // namespace detail

template<any_trait Trait>
class dyn_trait_ref : public decltype(detail::dyn_trait_ref_identity<Trait>::type_v)::type {
    dyn_trait_ref() = default;

    template<any_trait>
    friend class shared_trait_ptr;
    template<any_trait>
    friend class unique_trait_ptr;
    template<any_trait>
    friend class alloc_unique_trait_ptr;
    template<any_trait>
    friend class dyn_trait_ref;

    template<typename Impl, detail::any_dyn_trait_ref TraitRef>
        requires implements_trait<Impl, typename TraitRef::trait_t>
    friend auto is_holding_type(const TraitRef& ref) -> bool;
    template<typename Impl>
    [[nodiscard]] static auto is_holding_type(const dyn_trait_ref& ref) -> bool {
        return ref.vtable_ptr_->id_ptr == &detail::unique_id_struct<Impl>::value;
    }
    template<any_trait U>
        requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<Trait>>
    [[nodiscard]] friend constexpr auto is_valid_const_trait_cast(const dyn_trait_ref& ref) -> bool {
        if constexpr (supertrait_of<U, Trait>) {
            return true;
        } else {
            if constexpr (non_cv_trait<U>) {
                return ref.vtable_ptr_->cv_quals.has_full;
            } else if constexpr (std::is_const_v<U>) {
                return ref.vtable_ptr_->cv_quals.has_const;
            } else if constexpr (std::is_volatile_v<U>) {
                return ref.vtable_ptr_->cv_quals.has_volatile;
            }
            // otherwise U has to be a supertrait of Trait
        }
    }
    template<any_trait U>
        requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<Trait>>
    [[nodiscard]] friend constexpr auto const_trait_cast(const dyn_trait_ref& ref) -> dyn_trait_ref<U> {
        return {ref.vtable_ptr_, ref.obj_ptr_};
    }


    dyn_trait_ref(const detail::vtable<std::remove_cv_t<Trait>>* vptr, void* optr)
    : decltype(detail::dyn_trait_ref_identity<Trait>::type_v)::type(vptr, optr) {};

    template<implements_trait<Trait> Impl>
    explicit dyn_trait_ref(Impl* obj)
    : decltype(detail::dyn_trait_ref_identity<Trait>::type_v)::type(&detail::trait_vtable_for<Trait, Impl>,
                                                                    obj){};

public:
    template<any_trait U>
        requires explicit_supertrait_of<Trait, U> and (not std::same_as<Trait, U>)
    dyn_trait_ref(const dyn_trait_ref<U>& ref)    // NOLINT(*-explicit-*)
    : decltype(detail::dyn_trait_ref_identity<Trait>::type_v)::type(
          get_explicit_supertrait_vtable_ptr<std::remove_cv_t<Trait>>(ref.dyn_trait_ref_impl::vtable_ptr_),
          ref.dyn_trait_ref_impl::obj_ptr_){};

    template<implements_trait<Trait> Impl>
        requires(not detail::any_dyn_trait_ref<Impl>)
    explicit dyn_trait_ref(Impl& obj)
    : decltype(detail::dyn_trait_ref_identity<Trait>::type_v)::type(
          &detail::trait_vtable_for<std::remove_cv_t<Trait>, Impl>, &obj){};

    dyn_trait_ref(const dyn_trait_ref&)            = default;
    dyn_trait_ref(dyn_trait_ref&&)                 = default;
    dyn_trait_ref& operator=(const dyn_trait_ref&) = delete;
    dyn_trait_ref& operator=(dyn_trait_ref&&)      = delete;
    ~dyn_trait_ref()                               = default;
};

template<any_trait U, any_trait T>
    requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<T>>
[[nodiscard]] static constexpr auto is_valid_const_trait_cast(dyn_trait_ref<T> ref) -> bool {
    return is_valid_const_trait_cast<U>(ref);
}
template<any_trait U, any_trait T>
    requires std::same_as<std::remove_cv_t<U>, std::remove_cv_t<T>>
[[nodiscard]] static constexpr auto const_trait_cast(dyn_trait_ref<T> ref) {
    return const_trait_cast<U>(ref);
}

template<non_cv_trait Trait, template<typename> typename DefaultImpl = detail::empty_default_implementation>
    requires detail::strict_default_impl_for<DefaultImpl, Trait>
consteval void define_trait() {
    detail::maybe_define_cv_trait<Trait, DefaultImpl>();
};
}    // namespace trp
