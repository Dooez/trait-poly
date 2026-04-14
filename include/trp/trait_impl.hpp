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

template<non_ref T>
struct unique_id_struct {
    static inline char value{};
};

}    // namespace detail

template<any_trait Trait>
struct trait_ref;

namespace detail {
template<typename T>
concept any_trait_ref = meta::has_template_arguments(^^T) and meta::template_of(^^T) == ^^trait_ref;
}
template<typename Supertrait, detail::any_trait_ref TraitRef>
    requires explicit_supertrait_of<Supertrait, typename TraitRef::trait_t>
[[nodiscard]] auto trait_cast(const TraitRef& ref) {
    return ref.template upcast<Supertrait>();
}
template<typename Impl, detail::any_trait_ref TraitRef>
    requires implements_trait<Impl, typename TraitRef::trait_t>
[[nodiscard]] auto is_holding_type(const TraitRef& ref) -> bool {
    return TraitRef::template is_holding_type<Impl>(ref);
}

namespace detail {

template<any_method_idt Method>
using wrapper_fptr_for = Method::wrapper_fptr_type;

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
    for (auto method_idt: trait_traits<Trait>::all_methods) {
        vtable_elements.push_back(method_spec(method_idt));
    }
    using default_delete_fptr = void (*)(void*);
    vtable_elements.push_back(data_member_spec(^^default_delete_fptr, {.name = "default_delete"}));
    using id_ptr = const char*;
    vtable_elements.push_back(data_member_spec(^^id_ptr, {.name = "id_ptr"}));
    auto supertrait_spec = get_info_to_member(
        "supertrait_", [](info s) { return substitute(^^vtable, {copy_cv_to(^^Trait, s)}); });
    for (auto supertrait: direct_base_types<Trait>) {
        // not defining supertrait vtable because define_aggregate calls define_vtable for each trait in the hierarchy
        vtable_elements.push_back(supertrait_spec(supertrait));
    }
    define_aggregate(^^vtable<Trait>, vtable_elements);
}

template<any_trait Trait, non_cvref Impl>
consteval auto fill_vtable();

template<any_trait Trait, non_cvref Impl>
inline constexpr auto trait_vtable_for = fill_vtable<Trait, Impl>();

template<any_trait Trait, non_cvref Impl>
consteval auto fill_vtable() {
    using namespace std::meta;
    using ttt                      = trait_traits<Trait>;
    constexpr auto get_wrapper_ptr = [](cw_info auto trait_method_idt) {
        using trait_method_idt_t           = [:trait_method_idt:];
        constexpr auto matched_impl_method = [=] {
            for (auto m: matching_id_public_members<Impl, trait_method_idt_t::identifier>) {
                auto matches = meta::substitute(^^strictly_matches,
                                                {^^Impl, meta::reflect_constant(m), ^^trait_method_idt_t});
                if (meta::extract<bool>(matches))
                    return m;
            }
            std::unreachable();
        }();

        constexpr auto wrapper_struct_info = [=] {
            auto [... infos] = trait_method_idt_t::param_infos;
            return substitute(^^invoke_wrapper_struct,
                              {reflect_constant(matched_impl_method), trait_method_idt, infos...});
        }();
        using wrapper_struct = [:wrapper_struct_info:];
        return &wrapper_struct::invoke;
    };

    // use constexpr binding when it becomes available
    auto [... Is] = make_cw_idxs<ttt::all_methods.size()>();
    auto [... Js] = make_cw_idxs<direct_base_types<Trait>.size()>();
    return vtable<Trait>{
        get_wrapper_ptr(cw<ttt::all_methods[Is]>)...,
        &default_delete<Impl>,
        &unique_id_struct<Impl>::value,
        [:meta::substitute(^^trait_vtable_for,
                           {copy_cv_to(^^Trait, direct_base_types<Trait>[Js]), ^^Impl}):]...,
    };
}

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
            stdv::drop(stdr::size(trait_traits<Trait>::all_methods) + 1));
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
template<any_trait Trait>
consteval auto maybe_define_cv_trait() {
    using namespace std;
    using namespace meta;
    if (is_complete_type(substitute(^^trait_ref_identity, {^^Trait})))
        return;
    struct method_holder_spec {
        string_view  id;
        vector<info> targs;
    };
    using ttt = trait_traits<Trait>;
    template for (constexpr auto supertrait: direct_base_types<Trait>) {
        using supertrait_t = [:copy_cv_to(^^Trait, supertrait):];
        maybe_define_cv_trait<supertrait_t>();
    }
    define_vtable<Trait>();
    auto method_holder_specs = vector<method_holder_spec>{};
    method_holder_specs.reserve(ttt::all_methods.size());
    auto trait_ref_args = vector<info>{^^vtable<Trait>};

    uZ i = 0;
    template for (constexpr auto mem: ttt::all_methods) {
        const auto spec  = substitute(^^overload_spec, {reflect_constant(i), mem});
        using method_idt = [:mem:];

        auto it = stdr::find(method_holder_specs, method_idt::identifier, &method_holder_spec::id);
        if (it == method_holder_specs.end()) {
            const auto holder_info = substitute(^^method_holder, {^^Trait, reflect_constant(i)});
            method_holder_specs.push_back({
                .id = method_idt::identifier, .targs = {{}, holder_info, spec}
            });
            trait_ref_args.push_back(holder_info);
        } else {
            it->targs.push_back(spec);
        }
        ++i;
    }

    const auto mgr_info = substitute(^^trait_ref_impl, trait_ref_args);
    for (auto& [name, targs]: method_holder_specs) {
        targs[0]                = mgr_info;
        const auto invoker_type = substitute(^^method_invoker, targs);
        define_aggregate(targs[1],
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

template<typename VTable, typename... MethodHolders>
class trait_ref_impl : public MethodHolders... {
    using vtable_t = VTable;
    using trait_t  = [:meta::template_arguments_of(^^VTable)[0]:];

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
    template<typename Impl>
    [[nodiscard]] static auto is_holding_type(const trait_ref_impl& ref) -> bool {
        return ref.vtable_ptr_->id_ptr == &unique_id_struct<Impl>::value;
    }
    template<any_trait>
    friend class ::trp::trait_ref;
};

}    // namespace detail
template<any_trait Trait>
class trait_ref : public decltype(detail::trait_ref_identity<Trait>::type_v)::type {
    using trait_ref_impl = decltype(detail::trait_ref_identity<Trait>::type_v)::type;
    using vtable_t       = trait_ref_impl::vtable_t;

public:
    using trait_t = trait_ref_impl::trait_t;

private:
    trait_ref() = default;

    template<any_trait>
    friend class shared_trait_ptr;
    template<any_trait>
    friend class unique_trait_ptr;
    template<any_trait>
    friend class alloc_unique_trait_ptr;
    template<any_trait>
    friend class trait_ref;

    template<typename Supertrait, detail::any_trait_ref TraitRef>
        requires explicit_supertrait_of<Supertrait, typename TraitRef::trait_t>
    friend auto ::trp::trait_cast(const TraitRef&);
    template<explicit_supertrait_of<trait_t> Supertrait>
    [[nodiscard]] auto upcast() const -> trait_ref<Supertrait> {
        return trait_ref<Supertrait>(
            get_explicit_supertrait_vtable_ptr<Supertrait>(trait_ref_impl::vtable_ptr_),
            trait_ref_impl::obj_ptr_);
    }

    template<typename Impl, detail::any_trait_ref TraitRef>
        requires implements_trait<Impl, typename TraitRef::trait_t>
    friend auto is_holding_type(const TraitRef& ref) -> bool;
    template<typename Impl>
    [[nodiscard]] static auto is_holding_type(const trait_ref& ref) -> bool {
        return trait_ref_impl::template is_holding_type<Impl>(ref);
    }

    using trait_ref_impl::default_delete;
    using trait_ref_impl::holds_value;
    using trait_ref_impl::rebind;
    using trait_ref_impl::release;

    trait_ref(const vtable_t* vptr, void* optr)
    : trait_ref_impl(vptr, optr) {};

public:
    template<implements_trait<trait_t> Impl>
        requires(not std::derived_from<Impl, trait_ref_impl>)
    explicit trait_ref(Impl& obj)
    : trait_ref_impl(&detail::trait_vtable_for<trait_t, Impl>, &obj){};

    trait_ref(const trait_ref&)            = default;
    trait_ref(trait_ref&&)                 = default;
    trait_ref& operator=(const trait_ref&) = delete;
    trait_ref& operator=(trait_ref&&)      = delete;
    ~trait_ref()                           = default;
};


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
