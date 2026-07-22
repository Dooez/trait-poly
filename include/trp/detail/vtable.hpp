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

template<meta::info Fn, non_ref Impl, typename Ref, trait_method_idt MethodId, typename... Params>
struct explicit_invoke_wrapper_struct {
    using return_type = MethodId::return_type;

    static auto invoke(void* ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        Ref ref(*static_cast<Impl*>(ptr));
        if constexpr (MethodId::is_rvalue)
            return [:Fn:](std::move(ref), std::forward<Params>(params)...);
        else
            return [:Fn:](ref, std::forward<Params>(params)...);
    }
};

template<non_ref Impl, meta::info ImplMethod, trait_method_idt MethodId, typename... Params>
struct invoke_wrapper_struct {
    using return_type = MethodId::return_type;
    using obj_ptr     = [:add_pointer(MethodId::add_obj_cv(^^Impl)):];
    using obj_ref     = [:MethodId::add_obj_call_cvref(^^Impl):];

    static auto invoke(void* ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        decltype(auto) impl = static_cast<obj_ref>(*static_cast<obj_ptr>(ptr));
        if constexpr (is_template(ImplMethod)) {
            return std::forward<obj_ref>(impl).template[:ImplMethod:](std::forward<Params>(params)...);
        } else {
            return std::forward<obj_ref>(impl).[:ImplMethod:](std::forward<Params>(params)...);
        }
    }
};

template<typename Wrapper>
inline constexpr auto invoke_fptr = &Wrapper::invoke;

template<non_cv_trait Trait, non_cv_trait SESubtrait, non_ref Impl>
    requires explicit_supertrait_of<Trait, SESubtrait>
consteval auto fill_vtable();

template<non_cv_trait Trait, non_cv_trait SmallestExplicitSubtrait, non_ref Impl>
inline constexpr auto trait_vtable_for = fill_vtable<Trait, SmallestExplicitSubtrait, Impl>();


consteval auto get_wrapper_ptr(meta::info       se_subtrait,    //
                               meta::info       impl,
                               meta::info       method_idt,
                               vtable_cv_quals& quals) -> meta::info {
    constexpr auto find_compat = [](auto impls, meta::info method_idt) {
        auto const id           = std::string_view(extract_method_identifier(method_idt));
        auto const method_quals = extract_method_qualifiers(method_idt);
        auto const params       = extract_method_param_types(method_idt);
        auto const ret          = extract_method_return_type(method_idt);
        for (auto bind: impls) {
            auto [_, idt, _] = bind;
            if (id != extract_method_identifier(idt))
                continue;
            auto const q = extract_method_qualifiers(idt);
            if (q.is_const != method_quals.is_const or q.is_volatile != method_quals.is_volatile)
                continue;
            if (q.is_rvalue != method_quals.is_rvalue)
                continue;
            // q is subtrait method, and subtrait can only be more qualified
            // no need to check for lvalue becuase either method_quals.is_lvalue = false, which means relaxed matching
            // or quals.is_lvalue = true, which means subtrait must have lavlue qualified method

            auto const ps = extract_method_param_types(idt);
            if (not stdr::equal(params, ps))
                continue;
            if (ret != extract_method_return_type(idt))
                continue;
            return bind;
        }
        throw "Not found bind";
    };

    auto const bind =
        find_compat(subextract_span<impl_method_bind>(^^full_impls_for, {impl, se_subtrait}), method_idt);

    if (bind.fn == meta::info{}) {
        auto const method_quals = extract_method_qualifiers(method_idt);
        if (method_quals.is_const)
            quals.has_const = false;
        if (method_quals.is_volatile)
            quals.has_volatile = false;
        quals.has_full = false;
        return meta::reflect_constant(nullptr);
    } else {
        if (bind.is_explicit) {
            auto const cv_subtrait = add_method_obj_cv(bind.idt, se_subtrait);
            auto const cvts_ref_s  = substitute(^^cvts_trait_ref, {cv_subtrait, impl});
            auto const method      = substitute(bind.fn, {cvts_ref_s});

            auto wrapper_targs = std::vector{reflect_constant(method), impl, cvts_ref_s, bind.idt};
            wrapper_targs.append_range(extract_method_param_types(bind.idt));
            auto const wrapper_struct_info = substitute(^^explicit_invoke_wrapper_struct,    //
                                                        wrapper_targs);
            return substitute(^^invoke_fptr, {wrapper_struct_info});
        } else {
            auto wrapper_targs = std::vector{impl, reflect_constant(bind.fn), bind.idt};
            wrapper_targs.append_range(extract_method_param_types(bind.idt));
            auto const wrapper_struct_info = substitute(^^invoke_wrapper_struct, wrapper_targs);
            return substitute(^^invoke_fptr, {wrapper_struct_info});
        }
    }
};

template<non_cv_trait Trait, non_cv_trait SESubtrait, non_ref Impl>
    requires explicit_supertrait_of<Trait, SESubtrait>
consteval auto fill_vtable() {
    // use constexpr binding when it becomes available
    auto [... Is] = make_cw_idxs<all_trait_methods<Trait>.size()>();
    auto [... Js] = make_cw_idxs<direct_base_types<Trait>.size()>();
    auto quals    = vtable_cv_quals{};

    return vtable<Trait>{
        [:get_wrapper_ptr(^^SESubtrait,
                          ^^Impl,
                          all_trait_methods<Trait>[Is],
                          quals):]...,
                                  &default_delete<Impl>,
                                  &unique_id_struct<Impl>::value,
                                  quals,
                                  {[:substitute(^^trait_vtable_for,
                                                {direct_base_types<Trait>[Js], ^^SESubtrait, ^^Impl}):]...},
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
        using supertraits_t = decltype(vtable<Trait>::direct_supertraits);
        static constexpr auto mems =
            std::define_static_array(nonstatic_data_members_of(^^supertraits_t, unprivileged));
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
