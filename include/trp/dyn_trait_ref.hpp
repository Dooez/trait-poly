#pragma once
#ifndef TRP_GODBOLT
#include "detail/cvts_trait_ref.hpp"
#include "detail/vtable.hpp"
#endif

namespace trp {
template<any_trait Trait>
struct dyn_trait_ref;

namespace detail {
}

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

template<typename T>
concept any_dyn_trait_ref = meta::has_template_arguments(^^T) and meta::template_of(^^T) == ^^dyn_trait_ref;

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

template<any_trait Trait>
struct dyn_trait_ref_identity;

template<non_cv_trait Trait, template<typename> typename DefaultImpl>
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
