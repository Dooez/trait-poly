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
template<any_trait Trait, typename... MethodHolders>
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
    return upcast<Supertrait>(ref);
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
    template<typename... Args>
    auto operator()(Args&&... args) const
        volatile noexcept(noexcept_cvm_invoker<CVMInvoker, typename TRef::vtable_t, Args...>)
            -> decltype(auto) {
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

template<non_ref Impl, meta::info ImplMethod, any_method_idt MethodId, typename... Params>
struct invoke_wrapper_struct {
    using return_type    = MethodId::return_type;
    using erased_obj_ptr = [:meta::add_pointer(MethodId::add_obj_cv(^^void)):];
    using obj_ptr        = [:meta::add_pointer(MethodId::add_obj_cv(^^Impl)):];

    static auto invoke(erased_obj_ptr ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        auto&& impl = *static_cast<obj_ptr>(ptr);
        if constexpr (std::meta::is_template(ImplMethod)) {
            return impl.template[:ImplMethod:](std::forward<Params>(params)...);
        } else {
            return impl.[:ImplMethod:](std::forward<Params>(params)...);
        }
    }
};
template<non_cv_trait Trait>
struct vtable;

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

template<non_cv_trait Trait, non_ref Impl>
consteval auto fill_vtable();

template<non_cv_trait Trait, non_ref Impl>
inline constexpr auto trait_vtable_for = fill_vtable<Trait, Impl>();

template<non_cv_trait Trait, non_ref Impl>
consteval auto fill_vtable() {
    using namespace meta;
    using ttt                      = trait_traits<Trait>;
    constexpr auto get_wrapper_ptr = [](cw_info auto trait_method_idt) {
        using trait_method_idt_t           = [:trait_method_idt:];
        constexpr auto matched_impl_method = [=] {
            for (auto m: matching_id_public_members<std::remove_cv_t<Impl>, trait_method_idt_t::identifier>) {
                auto matches = meta::substitute(^^strictly_matches,
                                                {^^Impl, meta::reflect_constant(m), ^^trait_method_idt_t});
                if (meta::extract<bool>(matches))
                    return m;
            }
            return info{};
        }();

        // Common vtable is used for any combination of cv-qualification of trait.
        // Implementation is checked by externally.
        // Fill the missing vtable elements with nullptr.
        if (matched_impl_method == info{})
            return typename trait_method_idt_t::wrapper_fptr_type{nullptr};

        constexpr auto wrapper_struct_info = [=] {
            auto [... infos] = trait_method_idt_t::param_infos;
            return substitute(^^invoke_wrapper_struct,
                              {^^Impl, reflect_constant(matched_impl_method), trait_method_idt, infos...});
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
            stdv::drop(stdr::size(trait_traits<Trait>::all_methods)    //
                       + 1                                             // default deleter
                       + 1                                             // impl id
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
template<non_cv_trait Trait>
consteval auto maybe_define_cv_trait() {
    using namespace std;
    using namespace meta;
    if (is_complete_type(substitute(^^trait_ref_identity, {^^Trait})))
        return;
    struct method_holder_spec {
        string_view  id;
        info         holder_info;
        vector<info> cvm_invoker_targs;
    };
    using ttt = trait_traits<Trait>;
    template for (constexpr auto supertrait: direct_base_types<Trait>) {
        using supertrait_t = [:copy_cv_to(^^Trait, supertrait):];
        maybe_define_cv_trait<supertrait_t>();
    }
    define_vtable<Trait>();
    auto method_holders_specs    = vector<method_holder_spec>{};
    auto method_holders_specs_c  = vector<method_holder_spec>{};
    auto method_holders_specs_v  = vector<method_holder_spec>{};
    auto method_holders_specs_cv = vector<method_holder_spec>{};
    method_holders_specs.reserve(ttt::all_methods.size());
    method_holders_specs_c.reserve(ttt::all_methods.size());
    method_holders_specs_v.reserve(ttt::all_methods.size());
    method_holders_specs_cv.reserve(ttt::all_methods.size());

    auto trait_ref_args    = vector<info>{^^Trait};
    auto trait_ref_args_c  = vector<info>{^^Trait};
    auto trait_ref_args_v  = vector<info>{^^Trait};
    auto trait_ref_args_cv = vector<info>{^^Trait};

    uZ i = 0;
    template for (constexpr auto mem: ttt::all_methods) {
        using method_idt = [:mem:];
        auto add_holder  = [](auto& method_holders_specs, auto& trait_ref_args, auto trait_info, auto i) {
            const auto spec = substitute(^^overload_spec, {reflect_constant(i), mem});
            auto       it = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
            if (it == method_holders_specs.end()) {
                const auto holder_info = substitute(^^method_holder, {trait_info, reflect_constant(i)});
                method_holders_specs.push_back(
                    {.id = method_idt::identifier, .holder_info = holder_info, .cvm_invoker_targs = {spec}});
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
        const auto ref_info = substitute(^^trait_ref_impl, trait_ref_args);
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
            define_aggregate(substitute(^^trait_ref_identity, {trait_inf}),
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

template<any_trait Trait, typename... MethodHolders>
class trait_ref_impl : public MethodHolders... {
    using vtable_t = vtable<std::remove_cv_t<Trait>>;
    using trait_t  = Trait;

    const vtable_t* vtable_ptr_{};
    void*           obj_ptr_{};

    trait_ref_impl() = default;
    trait_ref_impl(const vtable_t* vptr, auto* optr)
    : vtable_ptr_(vptr)
    , obj_ptr_(
          (void*)optr)    // NOLINT(*cast*)cv qualification is kept via `invoke_wrapper_struct<...>::obj_ptr`
    {};

    template<typename, typename, typename>
    friend struct method_invoker;

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
    [[nodiscard]] friend auto upcast(const trait_ref& ref) -> trait_ref<Supertrait> {
        return trait_ref<Supertrait>(
            get_explicit_supertrait_vtable_ptr<Supertrait>(ref.trait_ref_impl::vtable_ptr_),
            ref.trait_ref_impl::obj_ptr_);
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

    template<implements_trait<trait_t> Impl>
    explicit trait_ref(Impl* obj)
    : trait_ref_impl(&detail::trait_vtable_for<trait_t, Impl>, obj){};

public:
    template<implements_trait<trait_t> Impl>
        requires(not std::derived_from<Impl, trait_ref_impl>)
    explicit trait_ref(Impl& obj)
    : trait_ref_impl(&detail::trait_vtable_for<std::remove_cv_t<trait_t>, Impl>, &obj){};

    trait_ref(const trait_ref&)            = default;
    trait_ref(trait_ref&&)                 = default;
    trait_ref& operator=(const trait_ref&) = delete;
    trait_ref& operator=(trait_ref&&)      = delete;
    ~trait_ref()                           = default;
};


template<non_cv_trait Trait>
consteval void define_trait() {
    detail::maybe_define_cv_trait<Trait>();
};
}    // namespace trp
