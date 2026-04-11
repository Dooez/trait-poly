#pragma once
#include <meta>
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
#include <algorithm>
#include <type_traits>


namespace trp {

namespace detail {
template<any_trait Trait>
struct trait_ref_identity;
template<typename VTable, typename... MethodHolders>
class trait_ref_impl;

template<typename T>
concept any_trait_ref = meta::has_template_arguments(^^T) and meta::template_of(^^T) == ^^trait_ref_impl;
}    // namespace detail

template<any_trait Trait>
using trait_ref = decltype(detail::trait_ref_identity<Trait>::type_v)::type;

template<typename Supertrait, detail::any_trait_ref TraitRef>
    requires explicit_supertrait_of<Supertrait, typename TraitRef::trait_t>
auto trait_cast(const TraitRef& ref) {
    return ref.template upcast<Supertrait>();
}

namespace detail {
template<any_trait Trait, non_cvref Impl>
struct trait_vtable_for;

template<typename VTable, typename... MethodHolders>
class trait_ref_impl : public MethodHolders... {
    using vtable_t = VTable;

public:
    using trait_t = [:meta::template_arguments_of(^^VTable)[0]:];

private:
    const vtable_t* vtable_ptr_{};
    void*           obj_ptr_{};

    trait_ref_impl() = default;
    trait_ref_impl(const vtable_t* vptr, void* optr)
    : vtable_ptr_(vptr)
    , obj_ptr_(optr) {};

    template<typename Manager,
             typename MethodHolder,
             typename MethodInvoker,
             uZ             Index,
             any_method_idt MethodId>
    friend struct overload_invoker;

    void release() {
        obj_ptr_ = nullptr;
    }

    void rebind(const trait_ref_impl& other) {
        vtable_ptr_ = other.vtable_ptr_;
        obj_ptr_    = other.obj_ptr_;
    }

    [[nodiscard]] bool holds_value() const {
        return obj_ptr_ != nullptr;
    }

    void default_delete() {
        vtable_ptr_->default_delete(obj_ptr_);
    }

    template<any_trait>
    friend class shared_trait_ptr_impl;
    template<any_trait>
    friend class unique_trait_ptr_impl;
    template<any_trait>
    friend class alloc_unique_trait_ptr_impl;
    template<typename, typename...>
    friend class trait_ref_impl;

    template<typename Supertrait, detail::any_trait_ref TraitRef>
        requires explicit_supertrait_of<Supertrait, typename TraitRef::trait_t>
    friend auto ::trp::trait_cast(const TraitRef&);

    template<explicit_supertrait_of<trait_t> Supertrait>
    [[nodiscard]] auto upcast() const -> trait_ref<Supertrait> {
        return trait_ref<Supertrait>(get_explicit_supertrait_vtable_ptr<Supertrait>(vtable_ptr_), obj_ptr_);
    }

public:
    trait_ref_impl(const trait_ref_impl&)            = default;
    trait_ref_impl(trait_ref_impl&&)                 = default;
    trait_ref_impl& operator=(const trait_ref_impl&) = delete;
    trait_ref_impl& operator=(trait_ref_impl&&)      = delete;
    ~trait_ref_impl()                                = default;

    template<implements_trait<trait_t> Impl>
        requires(not std::derived_from<Impl, trait_ref_impl>)
    explicit trait_ref_impl(Impl& obj)
    : vtable_ptr_(&detail::trait_vtable_for<trait_t, Impl>::value)
    , obj_ptr_(&obj){};
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
using wrapper_fptr_for = [:[] {
    using wrapper_fptr = [:[] {
        using return_type = typename Method::return_type;
        auto params       = std::vector{meta::reflect_constant(Method::is_const),    //
                                  meta::reflect_constant(Method::is_volatile),
                                  meta::reflect_constant(Method::is_noexcept),
                                  ^^return_type};
        params.append_range(Method::param_infos);
        return meta::substitute(^^wrapper_fptr, params);
    }():];
    return ^^typename wrapper_fptr::type;
}():];

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
        constexpr auto m = std::meta::nonstatic_data_members_of(^^VTable, ctx_unchecked)[Index];
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
template<any_trait Trait>
struct vtable;

template<any_trait Trait>
consteval void define_vtable() {
    using namespace meta;
    if (is_complete_type(^^vtable<Trait>))
        return;

    constexpr auto to_str = [](auto&& prefix, uZ i) -> std::string {
        auto res = std::string(prefix);
        res.resize(res.size() + 20);
        auto end = std::to_chars(&*res.begin() + 1, &*res.end(), i);
        if (end.ec != std::errc{})
            return {};
        res.resize(end.ptr - res.data());
        return res;
    };

    using default_delete_fptr = void (*)(void*);
    auto vtable_elements      = std::vector<info>{};
    uZ   i                    = 0;
    template for (constexpr auto method_idt: trait_traits<Trait>::all_methods) {
        using method_idt_t = [:method_idt:];
        vtable_elements.push_back(
            data_member_spec(substitute(^^wrapper_fptr_for, {method_idt}), {.name = to_str("m_", i++)}));
    }
    vtable_elements.push_back(data_member_spec(^^default_delete_fptr, {.name = "default_delete"}));
    i = 0;
    template for (constexpr auto supertrait: trait_traits<Trait>::direct_base_types) {
        // not defining supertrait vtable because define_aggregate calls define_vtable for each trait in the hierarchy
        auto supertrait_vtable = substitute(^^vtable, {copy_cv_to(^^Trait, supertrait)});
        vtable_elements.push_back(data_member_spec(supertrait_vtable, {.name = to_str("supertrait_", i++)}));
    }
    define_aggregate(^^vtable<Trait>, vtable_elements);
}

template<any_trait Trait, non_cvref Impl>
consteval auto fill_vtable() {
    using namespace std::meta;
    using ttt                      = trait_traits<Trait>;
    constexpr auto get_wrapper_ptr = [](cw_info auto trait_method_idt) {
        using trait_method_idt_t           = [:trait_method_idt:];
        constexpr auto matched_impl_method = [=] {
            constexpr auto matches = [=](cw_info auto impl_method) {
                return [:substitute(^^strictly_matches,
                                    {^^Impl, reflect_constant(impl_method), trait_method_idt}):];
            };
            static constexpr auto members = matching_id_public_members<Impl, trait_method_idt_t::identifier>;
            template for (constexpr auto m: members) {
                if (matches(cw<m>))
                    return m;
            }
            std::unreachable();
        }();

        // use constexpr binding when it becomes available
        auto [... trait_is] = make_cw_idxs<trait_method_idt_t::param_infos.size()>();

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
        using supertrait_t = [:copy_cv_to(^^Trait, supertriat):];
        return trait_vtable_for<supertrait_t, Impl>::value;
    };
    // use constexpr binding when it becomes available
    auto [... Is] = make_cw_idxs<ttt::all_methods.size()>();
    auto [... Js] = make_cw_idxs<ttt::direct_base_types.size()>();
    return vtable<Trait>{
        get_wrapper_ptr(cw<ttt::all_methods[Is]>)...,
        &default_delete<Impl>,
        get_supertrait_vtable(cw<ttt::direct_base_types[Js]>)...,
    };
}
template<any_trait Trait, non_cvref Impl>
struct trait_vtable_for {
    static constexpr auto value = fill_vtable<Trait, Impl>();
};

template<typename Supertrait, any_trait Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
constexpr auto get_explicit_supertrait_vtable_ptr(const vtable<Trait>* ptr) -> const vtable<Supertrait>* {
    if constexpr (std::same_as<Trait, Supertrait>)
        return ptr;

    using namespace meta;
    using next_supertrait_t = [:[] -> meta::info {
        if constexpr (direct_supertrait_of<Supertrait, Trait>) {
            return ^^Supertrait;
        } else {
            template for (constexpr auto base: trait_traits<Trait>::direct_base_types) {
                using base_t = [:base:];
                if constexpr (std::derived_from<base_t, Supertrait>)
                    return base;
            }
            std::unreachable();
        }
    }():];

    static constexpr auto vt_mems =
        ce_fn_to_array<[=] { return nonstatic_data_members_of(^^vtable<Trait>, ctx_unchecked); }>;
    const auto next_ptr = [=] -> const vtable<next_supertrait_t>* {
        template for (constexpr auto mem: vt_mems) {
            if constexpr (type_of(mem) == substitute(^^vtable, {^^next_supertrait_t}))
                return &((*ptr).[:mem:]);
        }
        std::unreachable();
    }();
    if constexpr (std::same_as<next_supertrait_t, Supertrait>) {
        return next_ptr;
    } else {
        return get_explicit_supertrait_vtable_ptr<Supertrait>(next_ptr);
    }
};
template<any_trait Trait>
consteval auto maybe_define_cv_trait() {
    using namespace std;
    using namespace meta;
    if (is_complete_type(substitute(^^trait_ref_identity, {^^Trait})))
        return;
    struct method_holder_spec {
        string_view  id;
        vector<info> methods;
        info         type_info{};
    };
    using ttt = trait_traits<Trait>;
    template for (constexpr auto supertrait: ttt::direct_base_types) {
        using supertrait_t = [:copy_cv_to(^^Trait, supertrait):];
        maybe_define_cv_trait<supertrait_t>();
    }
    define_vtable<Trait>();
    auto method_holder_specs = vector<method_holder_spec>{};
    auto trait_ref_args      = vector<info>{^^vtable<Trait>};

    uZ i = 0;
    template for (constexpr auto mem: ttt::all_methods) {
        const auto spec  = substitute(^^overload_spec, {reflect_constant(i), mem});
        using method_idt = [:mem:];

        auto it = stdr::find_if(method_holder_specs, [=](auto& p) { return p.id == method_idt::identifier; });
        if (it == method_holder_specs.end()) {
            const auto holder_info = substitute(^^method_holder, {^^Trait, reflect_constant(i)});
            method_holder_specs.push_back(
                {.id = method_idt::identifier, .methods = {spec}, .type_info = holder_info});
            trait_ref_args.push_back(holder_info);
        } else {
            it->methods.push_back(spec);
        }
        ++i;
    }

    const auto mgr_info = substitute(^^trait_ref_impl, trait_ref_args);
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
        define_aggregate(^^trait_ref_identity<Trait>,
                         {data_member_spec(type_v_info,
                                           data_member_options{
                                               .name              = "type_v",
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};

}    // namespace detail

template<any_trait Trait>
    requires non_cvref<Trait>
consteval void define_trait() {
    using c_trait  = [:meta::add_const(^^Trait):];
    using v_trait  = [:meta::add_volatile(^^Trait):];
    using cv_trait = [:meta::add_const(^^v_trait):];

    detail::maybe_define_cv_trait<Trait>();
    detail::maybe_define_cv_trait<c_trait>();
    detail::maybe_define_cv_trait<v_trait>();
    detail::maybe_define_cv_trait<cv_trait>();
};
}    // namespace trp
