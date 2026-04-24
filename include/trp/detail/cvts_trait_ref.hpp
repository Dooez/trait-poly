#pragma once
#ifndef TRP_GODBOLT
#include "default_trait_impl.hpp"
#endif

namespace trp::detail {
namespace cvts_trait {

template<typename Trait, typename Impl, typename... MethodHolders>
class cvts_trait_ref_impl : public MethodHolders... {
    Impl* _obj_ptr_{};

    template<typename, typename, typename, typename, meta::info, trait_method_idt>
    friend struct cvts_cvo_invoker;

public:
    explicit cvts_trait_ref_impl(Impl& optr)
    : _obj_ptr_(&optr) {};
};


// cv-transient static cv-overload invoker
template<typename TRef,
         typename MethodHolder,
         typename MethodInvoker,
         typename Impl,
         meta::info,
         trait_method_idt>
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
        if constexpr (Method == meta::info{}) {
            return [:default_method<>:](get_trait_ref()._obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref()._obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {
        if constexpr (Method == meta::info{}) {
            return [:default_method<>:](get_trait_ref()._obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref()._obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {
        if constexpr (Method == meta::info{}) {
            return [:default_method<>:](get_trait_ref()._obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref()._obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {
        if constexpr (Method == meta::info{}) {
            return [:default_method<>:](get_trait_ref()._obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref()._obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }

private:
    static auto mockfn(void*, Args&&...) -> Ret {
        std::unreachable();
    };    // required to make splic

    // template because TRef is incomplete at the point of cvts_cvo_invoker instantiation
    template<typename T = void>
    static constexpr auto default_method = [] {
        if (Method == meta::info{})
            return ^^mockfn;
        using trait_t = [:meta::remove_cv(meta::template_arguments_of(meta::type_of(
                              meta::bases_of(^^TRef, meta::access_context::unprivileged())[0]))[0]):];
        using method_idt =
            method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>;
        auto it = stdr::find(all_default_impls<trait_t>, ^^method_idt, &impl_method_bind::idt);
        return meta::substitute(it->fn, {^^TRef});
    }();

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

template<typename Impl, meta::info Method, trait_method_idt MethodId>
struct cvts_overload_spec {
    using impl_t                      = Impl;
    static constexpr auto impl_method = Method;
    using id                          = MethodId;
};

template<typename... OvSpecs>
struct ovspec_holder {};

// cv-transient static cv-method invoker
template<typename TRef, typename MethodHolder, typename OvSpecHolder>
struct cvts_cvm_invoker;

template<typename TRef, typename MethodHolder, typename... OvSpecs>
struct cvts_cvm_invoker<TRef, MethodHolder, ovspec_holder<OvSpecs...>>
: cvts_cvo_invoker<TRef,
                   MethodHolder,
                   cvts_cvm_invoker<TRef, MethodHolder, ovspec_holder<OvSpecs...>>,
                   typename OvSpecs::impl_t,
                   OvSpecs::impl_method,
                   typename OvSpecs::id>... {
    using cvts_cvo_invoker<TRef,
                           MethodHolder,
                           cvts_cvm_invoker<TRef, MethodHolder, ovspec_holder<OvSpecs...>>,
                           typename OvSpecs::impl_t,
                           OvSpecs::impl_method,
                           typename OvSpecs::id>::operator()...;
};


template<typename Ref, const char* id, typename OvSpecHolder>
inline constexpr auto cvts_holder = [] {
    struct cvts_method_holder;
    using invoker_t = cvts_cvm_invoker<Ref, cvts_method_holder, OvSpecHolder>;
    consteval {
        meta::define_aggregate(^^cvts_method_holder,
                               {meta::data_member_spec(^^invoker_t,
                                                       meta::data_member_options{
                                                           .name              = id,
                                                           .no_unique_address = true,
                                                           //  .attributes = {^^[[no_unique_address]] },
                                                       })});
    }
    return ^^cvts_method_holder;
}();

template<typename Trait, typename Impl>
consteval auto define_cvts_ref() {
    using namespace std;
    using namespace meta;
    struct ref;

    constexpr auto ref_impl = [] {
        struct method_holder_spec {
            const char*  id;
            vector<info> ov_specs;
        };
        auto method_holders_specs = vector<method_holder_spec>{};

        template for (constexpr auto mem: all_trait_methods<Trait>) {
            using method_idt    = [:mem:];
            constexpr auto spec = substitute(^^cvts_overload_spec,
                                             {^^Impl,    //
                                              reflect_constant(matching_impl_method<Impl, method_idt>),
                                              mem});
            auto it = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
            if (it == method_holders_specs.end()) {
                method_holders_specs.push_back({.id       = method_idt::identifier,    //
                                                .ov_specs = {spec}});
            } else {
                it->ov_specs.push_back(spec);
            }
        }
        auto impl_targs = vector<info>{^^Trait, ^^Impl};
        impl_targs.append_range(
            method_holders_specs    //
            | stdv::transform([](auto spec) {
                  return extract<info>(substitute(
                      ^^cvts_holder,
                      {^^ref, reflect_constant(spec.id), substitute(^^ovspec_holder, spec.ov_specs)}));
              }));

        return substitute(^^cvts_trait_ref_impl, impl_targs);
    }();
    using ref_impl_t = [:ref_impl:];
    struct ref : public ref_impl_t {
        ref(Impl& impl)
        : ref_impl_t(impl) {};
    };
    return ^^ref;
};

template<any_trait Trait, typename Impl>
inline constexpr auto cvts_ref_info = define_cvts_ref<Trait, Impl>();
}    // namespace cvts_trait

// cv-transient static trait reference
template<any_trait Trait, typename Impl>
using cvts_trait_ref = [:cvts_trait::cvts_ref_info<Trait, Impl>:];
}    // namespace trp::detail
