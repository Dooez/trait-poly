#pragma once
#ifndef TRP_GODBOLT
#include "default_trait_impl.hpp"
#endif

namespace trp::detail {

inline constexpr struct {
} vtable_ptr_anno;
inline constexpr struct {
} obj_ptr_anno;

consteval auto find_annotated_member(meta::info type, meta::info annotation) -> meta::info {
    for (auto m: nonstatic_data_members_of(type, meta::access_context::unchecked())) {
        for (auto ann: annotations_of(m))
            if (remove_cv(type_of(ann)) == remove_cv(type_of(annotation)))
                return m;
    };
    for (auto base: subextract_info_span(^^direct_base_types, {type})) {
        auto m = find_annotated_member(base, annotation);
        if (m != meta::info{})
            return m;
    }
    return {};
}

template<non_cvref TRef>
inline constexpr auto vtable_member_info = [] {
    auto m = find_annotated_member(^^TRef, ^^vtable_ptr_anno);
    if (m == meta::info{})
        throw "No vtable annotated member found";
    return m;
}();

template<non_cvref TRef>
inline constexpr auto obj_member_info = [] {
    auto m = find_annotated_member(^^TRef, ^^obj_ptr_anno);
    if (m == meta::info{})
        throw "No object annotated member found";
    return m;
}();

auto extract_vtable_ptr(auto&& ref) -> auto&&
    requires(vtable_member_info<std::remove_cvref_t<decltype(ref)>> != meta::info{})
{
    return ref.[:vtable_member_info<std::remove_cvref_t<decltype(ref)>>:];
}
auto extract_obj_ptr(auto&& ref) -> auto&&
    requires(obj_member_info<std::remove_cvref_t<decltype(ref)>> != meta::info{})
{
    return ref.[:obj_member_info<std::remove_cvref_t<decltype(ref)>>:];
}

namespace cvts_trait {

template<typename Trait, typename Impl, typename... MethodHolders>
class cvts_trait_ref_impl : public MethodHolders... {
protected:
    [[= obj_ptr_anno]] Impl* _;

    template<typename, typename, typename, typename, meta::info, trait_method_idt>
    friend struct cvts_cvo_invoker;

public:
    explicit cvts_trait_ref_impl(Impl& optr) {
        extract_obj_ptr(*this) = &optr;
    };
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
        if constexpr (explicit_method<> != meta::info{}) {
            return [:explicit_method<>:](get_trait_ref(), std::forward<Args>(args)...);
        } else {
            return extract_obj_ptr(get_trait_ref())->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const noexcept(Noexcept) -> Ret
        requires(Const and not Volatile)
    {
        if constexpr (explicit_method<> != meta::info{}) {
            return [:explicit_method<>:](get_trait_ref(), std::forward<Args>(args)...);
        } else {
            return extract_obj_ptr(get_trait_ref())->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) volatile noexcept(Noexcept) -> Ret
        requires(not Const and Volatile)
    {
        if constexpr (explicit_method<> != meta::info{}) {
            return [:explicit_method<>:](get_trait_ref(), std::forward<Args>(args)...);
        } else {
            return extract_obj_ptr(get_trait_ref())->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const volatile noexcept(Noexcept) -> Ret
        requires(Const and Volatile)
    {
        if constexpr (explicit_method<> != meta::info{}) {
            return [:explicit_method<>:](get_trait_ref(), std::forward<Args>(args)...);
        } else {
            return extract_obj_ptr(get_trait_ref())->[:Method:](std::forward<Args>(args)...);
        }
    }

private:
    // template because TRef is incomplete at the point of cvts_cvo_invoker instantiation
    template<typename T = void>
    static constexpr auto explicit_method = [] {
        if constexpr (is_template(Method)) {
            return substitute(Method, {^^TRef});
        } else if constexpr (concepts::explicit_method_impl_for<Method, Impl>) {
            return Method;
        } else {
            return meta::info{};
        }
    }();

    auto get_trait_ref(this auto&& self) -> decltype(auto) {
        constexpr auto add_cvp = [](meta::info type) {
            using this_t = std::remove_reference_t<decltype(self)>;
            return add_pointer(copy_cv_to(^^this_t, type));
        };

        static_assert(std::derived_from<MethodInvoker, cvts_cvo_invoker>);
        auto const mi_ptr = static_cast<[:add_cvp(^^MethodInvoker):]>(&self);

        constexpr auto invoker_ptr = [] {
            auto mems = nonstatic_data_members_of(^^MethodHolder, ctx_unchecked);
            if (mems.size() != 1)
                throw "Method holder is expected to have only a single method.";
            if (type_of(mems[0]) != ^^MethodInvoker)
                throw "Method invoker type does not match method holders first member type.";
            return extract<MethodInvoker MethodHolder::*>(mems[0]);
        }();
#ifdef __cpp_lib_is_pointer_interconvertible
        static_assert(std::is_pointer_interconvertible_with_class<MethodHolder>(invoker_ptr));
#endif
        static_assert(std::is_standard_layout_v<MethodHolder>);
        auto const mh_ptr = reinterpret_cast<[:add_cvp(^^MethodHolder):]>(mi_ptr);

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

template<typename Ref, char const* id, typename OvSpecHolder>
struct cvts_holder_definer {
    struct cvts_method_holder;
    using invoker_t = cvts_cvm_invoker<Ref, cvts_method_holder, OvSpecHolder>;
    consteval {
        define_aggregate(^^cvts_method_holder,
                         {data_member_spec(^^invoker_t,
                                           {
                                               .name              = id,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};

template<typename Ref, char const* id, typename OvSpecHolder>
using cvts_holder = typename cvts_holder_definer<Ref, id, OvSpecHolder>::cvts_method_holder;

template<typename Trait, typename Impl>
struct cvts_ref_definer {
    struct ref;
    struct ref_idt_holder;
    consteval {
        constexpr auto ref_impl = [] {
            struct method_holder_spec {
                char const*             id;
                std::vector<meta::info> ov_specs;
            };
            auto method_holders_specs = std::vector<method_holder_spec>{};

            template for (constexpr auto mem: all_trait_methods<Trait>) {
                using method_idt    = [:mem:];
                constexpr auto m    = stdr::find(impls_for<Impl, Trait>, mem, &impl_method_bind::idt)->fn;
                constexpr auto spec = substitute(^^cvts_overload_spec,
                                                 {^^Impl,    //
                                                  meta::reflect_constant(m),
                                                  mem});
                auto it = stdr::find(method_holders_specs, method_idt::identifier, &method_holder_spec::id);
                if (it == method_holders_specs.end()) {
                    method_holders_specs.push_back({.id       = method_idt::identifier,    //
                                                    .ov_specs = {spec}});
                } else {
                    it->ov_specs.push_back(spec);
                }
            }
            auto impl_targs = std::vector{^^Trait, ^^Impl};
            impl_targs.append_range(method_holders_specs    //
                                    | stdv::transform([](auto spec) {
                                          return substitute(^^cvts_holder,
                                                            {^^ref,
                                                             meta::reflect_constant(spec.id),
                                                             substitute(^^ovspec_holder, spec.ov_specs)});
                                      }));
            return substitute(^^cvts_trait_ref_impl, impl_targs);
        }();
        define_aggregate(^^ref_idt_holder,
                         {
                             data_member_spec(substitute(^^std::type_identity, {ref_impl}), {.name = "ref"}),
                         });
    }
    using ref_impl_t = decltype(ref_idt_holder::ref)::type;
    struct ref : public ref_impl_t {
        using ref_impl_t::ref_impl_t;
        operator Impl&() const volatile {
            return *extract_obj_ptr(*this);
        }
    };
};
}    // namespace cvts_trait

// cv-transient static trait reference
template<any_trait Trait, typename Impl>
using cvts_trait_ref = cvts_trait::cvts_ref_definer<Trait, Impl>::ref;
}    // namespace trp::detail
