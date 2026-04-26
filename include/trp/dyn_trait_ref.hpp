#pragma once
#ifndef TRP_GODBOLT
#include "detail/vtable.hpp"
#endif

namespace trp {
template<any_trait Trait>
struct dyn_trait_ref;

namespace detail {}

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

template<uZ Index, trait_method_idt MethodId>
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
        constexpr auto m = std::meta::nonstatic_data_members_of(^^typename VTable::vtable_impl, ctx_unchecked)[Index];
        return vt->[:m:];
    }
};
template<uZ I, trait_method_idt MethodId>
struct overload_spec {
    using id                  = MethodId;
    static constexpr uZ index = I;
};
template<typename... OvSpecs>
struct cvm_invoker : cvo_invoker<OvSpecs::index, typename OvSpecs::id>... {
    using cvo_invoker<OvSpecs::index, typename OvSpecs::id>::operator()...;
};

template<typename CVMInvoker, typename VTable, typename... Args>
concept noexcept_cvm_invoker =
    meta::is_template(^^CVMInvoker) and (meta::template_of(^^CVMInvoker) == ^^cvm_invoker) and ([] {
        CVMInvoker invoker{};
        VTable     vt{};
        return noexcept(invoker(&vt, nullptr, std::forward<Args>(std::declval<Args>())...));
    });


template<typename TRef, typename Trait, typename MethodHolder, typename CVMInvoker>
struct method_invoker {
private:
    using trait_t  = Trait;
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

template<const char* id, typename Ref, typename Trait, typename CVMInvoker>
struct cvm_holder_definer {
    struct cvm_holder;
    consteval {
        constexpr auto method_invoker_info =
            substitute(^^method_invoker, {^^Ref, ^^Trait, ^^cvm_holder, ^^CVMInvoker});
        define_aggregate(^^cvm_holder,
                         {data_member_spec(method_invoker_info,
                                           meta::data_member_options{
                                               .name              = id,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};
template<const char* id, typename Ref, typename Trait, typename CVMInvoker>
inline constexpr auto cvm_holder_info = ^^typename cvm_holder_definer<id, Ref, Trait, CVMInvoker>::cvm_holder;

consteval auto extract_cvm_holder(meta::info id, meta::info ref, meta::info cvm_invoker) {}

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

    template<typename, typename, typename, typename>
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
template<non_cv_trait Trait>
struct dyn_cv_ref_impls {
    struct ref;
    struct ref_c;
    struct ref_v;
    struct ref_cv;

    struct all_refs;
    consteval {
        using namespace std;

        struct method_holder_spec {
            const char*        id;
            vector<meta::info> cvm_invoker_targs;
        };

        constexpr auto impls = [] {
            auto method_holders_specs    = vector<method_holder_spec>{};
            auto method_holders_specs_c  = vector<method_holder_spec>{};
            auto method_holders_specs_v  = vector<method_holder_spec>{};
            auto method_holders_specs_cv = vector<method_holder_spec>{};
            method_holders_specs.reserve(all_trait_methods<Trait>.size());
            method_holders_specs_c.reserve(all_trait_methods<Trait>.size());
            method_holders_specs_v.reserve(all_trait_methods<Trait>.size());
            method_holders_specs_cv.reserve(all_trait_methods<Trait>.size());

            uZ i = 0;
            template for (constexpr auto mem: all_trait_methods<Trait>) {
                using method_idt      = [:mem:];
                const auto spec       = substitute(^^overload_spec, {meta::reflect_constant(i), mem});
                auto       add_holder = [=](auto& method_holders_specs, auto trait_info) {
                    auto it =
                        stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
                    if (it == method_holders_specs.end()) {
                        method_holders_specs.push_back(
                            {.id = method_idt::identifier, .cvm_invoker_targs = {spec}});
                    } else {
                        it->cvm_invoker_targs.push_back(spec);
                    }
                };
                add_holder(method_holders_specs, ^^Trait);
                if (method_idt::is_const)
                    add_holder(method_holders_specs_c, meta::add_const(^^Trait));
                if (method_idt::is_volatile)
                    add_holder(method_holders_specs_v, meta::add_volatile(^^Trait));
                if (method_idt::is_cv)
                    add_holder(method_holders_specs_cv, meta::add_cv(^^Trait));
                ++i;
            }

            constexpr auto get_ref_impl =
                [](auto& method_holders_specs, meta::info trait_inf, meta::info ref_info) {
                    auto ref_impl_targs = vector<meta::info>{trait_inf};
                    for (auto& [id, cvm_invoker_targs]: method_holders_specs) {
                        const auto cvm_invoker_info = substitute(^^cvm_invoker, cvm_invoker_targs);
                        ref_impl_targs.push_back(extract<meta::info>(
                            substitute(^^cvm_holder_info,
                                       {meta::reflect_constant(id), ref_info, trait_inf, cvm_invoker_info})));
                    }
                    return substitute(^^dyn_trait_ref_impl, ref_impl_targs);
                };
            return std::define_static_array(std::initializer_list{
                get_ref_impl(method_holders_specs, ^^Trait, ^^ref),
                get_ref_impl(method_holders_specs_c, meta::add_const(^^Trait), ^^ref_c),
                get_ref_impl(method_holders_specs_v, meta::add_volatile(^^Trait), ^^ref_v),
                get_ref_impl(method_holders_specs_cv, meta::add_cv(^^Trait), ^^ref_cv),
            });
        }();
        using impl_t    = [:impls[0]:];
        using impl_c_t  = [:impls[1]:];
        using impl_v_t  = [:impls[2]:];
        using impl_cv_t = [:impls[3]:];
        define_aggregate(
            ^^all_refs,
            {
                meta::data_member_spec(substitute(^^type_identity, {impls[0]}), {.name = "ref"}),
                meta::data_member_spec(substitute(^^type_identity, {impls[1]}), {.name = "ref_c"}),
                meta::data_member_spec(substitute(^^type_identity, {impls[2]}), {.name = "ref_v"}),
                meta::data_member_spec(substitute(^^type_identity, {impls[3]}), {.name = "ref_cv"}),
            });
    }
    using impl_t    = decltype(all_refs::ref)::type;
    using impl_c_t  = decltype(all_refs::ref_c)::type;
    using impl_v_t  = decltype(all_refs::ref_v)::type;
    using impl_cv_t = decltype(all_refs::ref_cv)::type;

    struct ref : public impl_t {
        using impl_t::impl_t;
    };
    struct ref_c : public impl_c_t {
        using impl_c_t::impl_c_t;
    };
    struct ref_v : public impl_v_t {
        using impl_v_t::impl_v_t;
    };
    struct ref_cv : public impl_cv_t {
        using impl_cv_t::impl_cv_t;
    };
};
template<any_trait Trait>
using dyn_ref_impl = [:[] {
    using impls = dyn_cv_ref_impls<std::remove_cv_t<Trait>>;
    if constexpr (std::is_const_v<Trait> and std::is_volatile_v<Trait>) {
        return ^^typename impls::ref_cv;
    } else if constexpr (std::is_volatile_v<Trait>) {
        return ^^typename impls::ref_v;
    } else if constexpr (std::is_const_v<Trait>) {
        return ^^typename impls::ref_c;
    } else {
        return ^^typename impls::ref;
    }
}():];


}    // namespace detail

template<any_trait Trait>
class dyn_trait_ref : public detail::dyn_ref_impl<Trait> {
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
    : detail::dyn_ref_impl<Trait>(vptr, optr) {};

    template<implements_trait<Trait> Impl>
    explicit dyn_trait_ref(Impl* obj)
    : detail::dyn_ref_impl<Trait>(
          &detail::trait_vtable_for<std::remove_cv_t<Trait>, std::remove_cv_t<Trait>, Impl>, obj){};

public:
    template<any_trait U>
        requires explicit_supertrait_of<Trait, U> and (not std::same_as<Trait, U>)
    dyn_trait_ref(const dyn_trait_ref<U>& ref)    // NOLINT(*-explicit-*)
    : detail::dyn_ref_impl<Trait>(
          get_explicit_supertrait_vtable_ptr<std::remove_cv_t<Trait>>(ref.dyn_trait_ref_impl::vtable_ptr_),
          ref.dyn_trait_ref_impl::obj_ptr_){};

    template<implements_trait<Trait> Impl>
        requires(not detail::any_dyn_trait_ref<Impl>)
    explicit dyn_trait_ref(Impl& obj)
    : detail::dyn_ref_impl<Trait>(
          &detail::trait_vtable_for<std::remove_cv_t<Trait>, std::remove_cv_t<Trait>, Impl>, &obj){};

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
}    // namespace trp
