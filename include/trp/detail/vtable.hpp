#pragma once
#ifndef TRP_GODBOLT
#include "cvts_trait_ref.hpp"
#include "default_trait_impl.hpp"
#endif

namespace trp {
namespace detail {

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
inline constexpr auto vtable_info = [] {
    using namespace meta;

    struct vtable;

    consteval{
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
    for (auto method_idt: all_trait_methods<Trait>) {
        vtable_elements.push_back(method_spec(method_idt));
    }
    using default_delete_fptr = void (*)(void*);
    vtable_elements.push_back(data_member_spec(^^default_delete_fptr, {.name = "default_delete"}));
    using id_ptr = const char*;
    vtable_elements.push_back(data_member_spec(^^id_ptr, {.name = "id_ptr"}));
    vtable_elements.push_back(data_member_spec(^^vtable_cv_quals, {.name = "cv_quals"}));

    auto supertrait_spec = get_info_to_member("supertrait_", [](info s) {
        return extract<meta::info>(substitute(^^vtable_info, {copy_cv_to(^^Trait, s)}));
    });
    for (auto supertrait: direct_base_types<Trait>) {
        // not defining supertrait vtable because maybe_define_cv_trait calls define_vtable for each trait in the hierarchy
        vtable_elements.push_back(supertrait_spec(supertrait));
    }
    define_aggregate(^^vtable, vtable_elements);
    }
    return ^^vtable;
}();
template<non_cv_trait Trait>
using vtable = [:vtable_info<Trait>:];

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
    using obj_ptr     = [:meta::add_pointer(MethodId::add_obj_cv(^^Impl)):];

    static auto invoke(void* ptr, Params... params) noexcept(MethodId::is_noexcept) -> return_type {
        auto&& impl = *static_cast<obj_ptr>(ptr);
        if constexpr (std::meta::is_template(ImplMethod)) {
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
    using namespace meta;
    auto       quals           = vtable_cv_quals{};
    const auto get_wrapper_ptr = [&](cw_info auto trait_method_idt) {
        using trait_method_idt_t = [:trait_method_idt:];
        constexpr auto m =
            stdr::find(impls_for<Impl, SESubtrait>, trait_method_idt, &impl_method_bind::idt)->fn;
        if (m == info{}) {
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
            constexpr auto method              = meta::substitute(m, {^^cvts_ref});
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
        [:meta::substitute(^^trait_vtable_for, {direct_base_types<Trait>[Js], ^^Trait, ^^Impl}):]...,
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
            stdv::drop(stdr::size(all_trait_methods<Trait>)    //
                       + 1                                     // default_deleter
                       + 1                                     // id_ptr
                       + 1                                     // cv_quals
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

}    // namespace detail
}    // namespace trp
