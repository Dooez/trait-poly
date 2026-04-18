#pragma once
#include <meta>
#include <ranges>
#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
#include <algorithm>
#include <type_traits>

namespace trp::detail {
template<non_cv_trait Trait>
struct default_trait_impl_identity;

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

template<any_trait Trait, typename Impl>
struct cvts_trait_ref_identity;

template<any_trait Trait, typename Impl, typename... MethodHolders>
class cvts_trait_ref_impl;

// cv-transient static trait reference
template<any_trait Trait, typename Impl>
using cvts_trait_ref = decltype(cvts_trait_ref_identity<Trait, Impl>::type_v)::type;

template<typename T>
concept any_cvts_trait_ref = meta::has_template_arguments(^^T) and meta::template_of(^^T) == ^^cvts_trait_ref;

template<any_method_idt>
struct cvtmock_cvo_invoker;
template<auto Identifier,
         bool Const,
         bool Volatile,
         bool LVRef,
         bool RVRef,
         bool Value,
         bool Noexcept,
         typename Ret,
         typename... Args>
    requires(not(LVRef or RVRef or Value))    // not supported
struct cvtmock_cvo_invoker<
    method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>> {
    auto operator()(Args... args) noexcept(Noexcept) -> Ret
        requires(not Const and not Volatile)
    {}
    auto operator()(Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {}
    auto operator()(Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {}
    auto operator()(Args... args) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {}
};

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
        if constexpr (Method == meta::info{}) {
            [:default_method<>:](get_trait_ref().obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {
        if constexpr (Method == meta::info{}) {
            [:default_method<>:](get_trait_ref().obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {
        if constexpr (Method == meta::info{}) {
            [:default_method<>:](get_trait_ref().obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {
        if constexpr (Method == meta::info{}) {
            [:default_method<>:](get_trait_ref().obj_ptr_, std::forward<Args>(args)...);
        } else {
            return get_trait_ref().obj_ptr_->[:Method:](std::forward<Args>(args)...);
        }
    }

private:
    template<typename T = void>
    static constexpr auto default_method = [] {
        using trait_t = [:meta::template_arguments_of(
                              meta::bases_of(^^TRef, meta::access_context::unprivileged())[0])[0]:];
        using method_idt =
            method_identity_t<Identifier, Const, Volatile, LVRef, RVRef, Value, Noexcept, Ret, Args...>;
        static constexpr meta::info default_impl_template =
            decltype(default_trait_impl_identity<trait_t>::template_info)::value;
        using cvts_ref           = [:define_cvts_ref<trait_t, Impl>():];
        using default_trait_impl = [:meta::substitute(default_impl_template, {^^cvts_ref}):];
        return *stdr::find(
            nonspecial_members<default_trait_impl>, ^^method_idt, default_impl_to_method_identity);
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
template<any_method_idt... MethodIds>
struct cvtmock_cvm_invoker : cvtmock_cvo_invoker<MethodIds>... {
    using cvtmock_cvo_invoker<MethodIds>::operator()...;
};

template<typename Impl, meta::info Method, any_method_idt MethodId>
struct cvts_overload_spec {
    using impl_t                      = Impl;
    static constexpr auto impl_method = Method;
    using id                          = MethodId;
};

template<typename... OvSpecs>
struct ovspec_holder {};

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

template<auto HolderSpec>
consteval auto get_cvtmock_holder() {
    struct cvtmock_method_holder;
    consteval {
        meta::define_aggregate(^^cvtmock_method_holder,
                               {meta::data_member_spec(HolderSpec.invoker_info,
                                                       meta::data_member_options{
                                                           .name              = HolderSpec.id,
                                                           .no_unique_address = true,
                                                           //  .attributes = {^^[[no_unique_address]] },
                                                       })});
    }
    return ^^cvtmock_method_holder;
}

template<typename Ref, auto HolderSpec>
consteval auto get_cvts_holder() {
    struct cvts_method_holder;
    using invoker_t = [:substitute(^^cvts_cvm_invoker,
                                   {^^Ref, ^^cvts_method_holder, HolderSpec.ovspec_holder}):];

    consteval {
        meta::define_aggregate(^^cvts_method_holder,
                               {meta::data_member_spec(^^invoker_t,
                                                       meta::data_member_options{
                                                           .name              = HolderSpec.id,
                                                           .no_unique_address = true,
                                                           //  .attributes = {^^[[no_unique_address]] },
                                                       })});
    }
    return ^^cvts_method_holder;
}
template<typename... MethodHolders>
struct mock_ref_impl : public MethodHolders... {};

template<typename Trait>
consteval auto define_cvtmock_ref() {
    using namespace std;
    using namespace meta;

    constexpr auto method_holders_specs = [] {
        struct method_holder_spec {
            const char*  id;
            vector<info> method_idts;
        };
        auto method_holders_specs = vector<method_holder_spec>{};

        template for (constexpr auto mem: all_trait_methods<Trait>) {
            using method_idt = [:mem:];
            auto it = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
            if (it == method_holders_specs.end()) {
                method_holders_specs.push_back({.id          = method_idt::identifier,    //
                                                .method_idts = {mem}});
            } else {
                it->method_idts.push_back(mem);
            }
        }
        struct method_holder_prep {
            const char* id;
            info        invoker_info;
        };
        return std::define_static_array(method_holders_specs | stdv::transform([](auto spec) {
                                            return method_holder_prep{
                                                spec.id, substitute(^^cvtmock_cvm_invoker, spec.method_idts)};
                                        }));
    }();

    auto trait_ref_args = vector<info>{};
    template for (constexpr auto spec: method_holders_specs) {
        trait_ref_args.push_back(get_cvtmock_holder<spec>());
    }
    return substitute(^^mock_ref_impl, trait_ref_args);
};
template<typename Trait>
inline constexpr auto defined_mock_ref_info = define_cvtmock_ref<Trait>();
template<typename Trait>
using mock_ref = [:defined_mock_ref_info<Trait>:];


template<typename Trait, typename Impl>
consteval auto define_cvts_ref() {
    using namespace std;
    using namespace meta;

    struct ref;

    constexpr auto method_holders_specs = [] {
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
        struct method_holder_prep {
            const char* id;
            info        ovspec_holder;
        };
        return std::define_static_array(method_holders_specs | stdv::transform([](auto spec) {
                                            return method_holder_prep{
                                                spec.id, substitute(^^ovspec_holder, spec.ov_specs)};
                                        }));
    }();

    constexpr auto ref_impl = [=] {
        auto trait_ref_args = vector<info>{^^Trait, ^^Impl};
        template for (constexpr auto holder_spec: method_holders_specs) {
            trait_ref_args.push_back(get_cvts_holder<ref, holder_spec>());
        }
        return substitute(^^cvts_trait_ref_impl, trait_ref_args);
    }();
    using ref_impl_t = [:ref_impl:];
    struct ref : public ref_impl_t {
        ref(Impl& impl)
        : ref_impl_t(impl) {};
    };
    return ^^ref;
};

template<typename Trait, typename Impl>
inline constexpr auto defined_cvts_ref_info = define_cvts_ref<Trait, Impl>();

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
