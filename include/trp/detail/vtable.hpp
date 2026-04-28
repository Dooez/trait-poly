#pragma once
#ifndef TRP_GODBOLT
#include "cvts_trait_ref.hpp"
#include "default_trait_impl.hpp"
#endif

namespace trp::detail {

template<typename Impl>
void default_delete(void* ptr) {
    delete static_cast<Impl*>(ptr);
}

struct vtable_cv_quals {
    bool has_full     = true;
    bool has_const    = true;
    bool has_volatile = true;
    // bool has_cv=true; not needed since this is a biggest supertrait
};

template<trait_method_idt Method>
using wrapper_fptr_for = Method::wrapper_fptr_type;

template<typename VTableDefiner>
inline constexpr auto vtable_extractor = ^^typename VTableDefiner::vtable;

template<non_cv_trait Trait>
struct vtable_definer;
template<non_cv_trait Trait>
struct vtable : public vtable_definer<Trait>::vtable_impl {
    using vtable_impl = vtable_definer<Trait>::vtable_impl;
};

template<non_cv_trait Trait>
struct vtable_definer {
    struct vtable_impl;
    consteval {
        auto vtable_elements = all_trait_methods<Trait>    //
                               | stdv::transform([](auto m) {
                                     return anon_member_spec(substitute(^^wrapper_fptr_for, {m}));
                                 })    //
                               | stdr::to<std::vector<meta::info>>();
        using default_delete_fptr = void (*)(void*);
        vtable_elements.push_back(data_member_spec(^^default_delete_fptr, {.name = "default_delete"}));
        using id_ptr = char const*;
        vtable_elements.push_back(data_member_spec(^^id_ptr, {.name = "id_ptr"}));
        vtable_elements.push_back(data_member_spec(^^vtable_cv_quals, {.name = "cv_quals"}));
        auto direct_supertraits_t =
            substitute(^^anon_aggregate, direct_base_types<Trait> | stdv::transform([](auto s) {
                                             return substitute(^^detail::vtable, {s});
                                         }));
        vtable_elements.push_back(data_member_spec(direct_supertraits_t, {.name = "direct_supertraits"}));
        define_aggregate(^^vtable_impl, vtable_elements);
    }
};

template<meta::info Fn,
         non_ref    Impl,
         typename Ref,
         any_trait        Trait,
         trait_method_idt MethodId,
         typename... Params>
struct explicit_invoke_wrapper_struct {
    using return_type = MethodId::return_type;

    static auto invoke(void* ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        Ref ref(*static_cast<Impl*>(ptr));
        return [:Fn:](ref, std::forward<Params>(params)...);
    }
};

template<non_ref Impl, meta::info ImplMethod, trait_method_idt MethodId, typename... Params>
struct invoke_wrapper_struct {
    using return_type = MethodId::return_type;
    using obj_ptr     = [:add_pointer(MethodId::add_obj_cv(^^Impl)):];

    static auto invoke(void* ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        auto&& impl = *static_cast<obj_ptr>(ptr);
        if constexpr (is_template(ImplMethod)) {
            return impl.template[:ImplMethod:](std::forward<Params>(params)...);
        } else {
            return impl.[:ImplMethod:](std::forward<Params>(params)...);
        }
    }
};


template<non_cv_trait Trait, non_cv_trait SESubtrait, non_ref Impl>
    requires explicit_supertrait_of<Trait, SESubtrait>
consteval auto fill_vtable();

template<non_cv_trait Trait, non_cv_trait SmallestExplicitSubtrait, non_ref Impl>
inline constexpr auto trait_vtable_for = fill_vtable<Trait, SmallestExplicitSubtrait, Impl>();

template<non_cv_trait Trait, non_cv_trait SESubtrait, non_ref Impl>
    requires explicit_supertrait_of<Trait, SESubtrait>
consteval auto fill_vtable() {
    auto       quals           = vtable_cv_quals{};
    auto const get_wrapper_ptr = [&](cw_info auto trait_method_idt) {
        using trait_method_idt_t = [:trait_method_idt:];
        constexpr auto m =
            stdr::find(impls_for<Impl, SESubtrait>, meta::info{trait_method_idt}, &impl_method_bind::idt)->fn;
        if (m == meta::info{}) {
            if (trait_method_idt_t::is_const)
                quals.has_const = false;
            if (trait_method_idt_t::is_volatile)
                quals.has_volatile = false;
            quals.has_full = false;
            return typename trait_method_idt_t::wrapper_fptr_type{nullptr};
        }

        auto [... is] = make_cw_idxs<trait_method_idt_t::param_infos.size()>();
        if constexpr (concepts::explicit_template_method_impl_of<m, Trait>) {    // default impl
            using cvts_ref                     = cvts_trait_ref<Trait, Impl>;
            constexpr auto method              = substitute(m, {^^cvts_ref});
            constexpr auto wrapper_struct_info = substitute(^^explicit_invoke_wrapper_struct,    //
                                                            {reflect_constant(method),
                                                             ^^Impl,
                                                             ^^cvts_ref,
                                                             ^^Trait,
                                                             trait_method_idt,
                                                             trait_method_idt_t::param_infos[is]...});
            using wrapper_struct               = [:wrapper_struct_info:];
            return &wrapper_struct::invoke;

        } else if constexpr (concepts::explicit_method_impl_for<m, Impl>) {    // explicit spec
            constexpr auto ref                 = type_of(parameters_of(m)[0]);
            constexpr auto wrapper_struct_info = substitute(^^explicit_invoke_wrapper_struct,    //
                                                            {reflect_constant(m),
                                                             ^^Impl,
                                                             ref,
                                                             ^^Trait,
                                                             trait_method_idt,
                                                             trait_method_idt_t::param_infos[is]...});
            using wrapper_struct               = [:wrapper_struct_info:];
            return &wrapper_struct::invoke;

        } else {
            if (parent_of(m) != ^^Impl)
                throw "Not parent";
            constexpr auto cm                  = reflect_constant(m);
            constexpr auto wrapper_struct_info = substitute(
                ^^invoke_wrapper_struct,
                {^^Impl, reflect_constant(m), trait_method_idt, trait_method_idt_t::param_infos[is]...});
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
        {[:substitute(^^trait_vtable_for, {direct_base_types<Trait>[Js], ^^Trait, ^^Impl}):]...},
    };
}
template<non_cv_trait Supertrait, non_cv_trait Trait>
    requires explicit_supertrait_of<Supertrait, Trait>
constexpr auto get_explicit_supertrait_vtable_ptr(vtable<Trait> const* ptr) -> vtable<Supertrait> const* {
    if constexpr (std::same_as<Trait, Supertrait>)
        return ptr;

    using next_supertrait_t = [:[] -> meta::info {
        if constexpr (direct_supertrait_of<Supertrait, Trait>) {
            return ^^Supertrait;
        } else {
            for (auto base: direct_base_types<Trait>) {
                auto const is_derived = substitute(^^std::derived_from, {base, ^^Supertrait});
                if (extract<bool>(is_derived))
                    return base;
            }
            std::unreachable();
        }
    }():];

    auto const next_ptr = [=] -> vtable<next_supertrait_t> const* {
        using supertraits_t        = decltype(vtable<Trait>::direct_supertraits);
        static constexpr auto mems = std::define_static_array(
            nonstatic_data_members_of(^^supertraits_t, meta::access_context::unprivileged()));
        template for (constexpr auto m: mems) {
            if constexpr (type_of(m) == substitute(^^vtable, {^^next_supertrait_t}))
                return &(ptr->direct_supertraits.[:m:]);
        }
        std::unreachable();
    }();
    if constexpr (std::same_as<next_supertrait_t, Supertrait>) {
        return next_ptr;
    } else {
        return get_explicit_supertrait_vtable_ptr<Supertrait>(next_ptr);
    }
};
}    // namespace trp::detail
