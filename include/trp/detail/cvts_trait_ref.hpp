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
         meta::info Method,
         trait_method_idt>
struct cvts_cvo_invoker;

template<typename TRef,
         typename MethodHolder,
         typename MethodInvoker,
         typename Impl,
         meta::info          Method,
         auto                Identifier,
         method_qualifiers_t Quals,
         typename Ret,
         typename... Args>
struct cvts_cvo_invoker<TRef,
                        MethodHolder,
                        MethodInvoker,
                        Impl,
                        Method,
                        method_identity_t<Identifier, Quals, Ret, Args...>> {
    auto operator()(Args... args) noexcept(Quals.is_noexcept) -> Ret
        requires(not Quals.is_const and not Quals.is_volatile)
    {
        if constexpr (explicit_method<> != meta::info{}) {
            return [:explicit_method<>:](get_trait_ref(), std::forward<Args>(args)...);
        } else {
            return extract_obj_ptr(get_trait_ref())->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const noexcept(Quals.is_noexcept) -> Ret
        requires(Quals.is_const and not Quals.is_volatile)
    {
        if constexpr (explicit_method<> != meta::info{}) {
            return [:explicit_method<>:](get_trait_ref(), std::forward<Args>(args)...);
        } else {
            return extract_obj_ptr(get_trait_ref())->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) volatile noexcept(Quals.is_noexcept) -> Ret
        requires(not Quals.is_const and Quals.is_volatile)
    {
        if constexpr (explicit_method<> != meta::info{}) {
            return [:explicit_method<>:](get_trait_ref(), std::forward<Args>(args)...);
        } else {
            return extract_obj_ptr(get_trait_ref())->[:Method:](std::forward<Args>(args)...);
        }
    }
    auto operator()(Args... args) const volatile noexcept(Quals.is_noexcept) -> Ret
        requires(Quals.is_const and Quals.is_volatile)
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

template<char const* id, typename Ref, typename OvSpecHolder>
struct cvts_holder_definer {
    struct cvts_method_holder;
    consteval {
        using invoker_t = cvts_cvm_invoker<Ref, cvts_method_holder, OvSpecHolder>;
        define_aggregate(^^cvts_method_holder,
                         {data_member_spec(^^invoker_t,
                                           {
                                               .name              = id,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};

template<char const* id, typename Ref, typename OvSpecHolder>
using cvts_holder = typename cvts_holder_definer<id, Ref, OvSpecHolder>::cvts_method_holder;

template<non_cvref Trait, typename Impl>
struct cvts_ref_definer {
    struct ref;
    struct ref_c;
    struct ref_v;
    struct ref_cv;

    static constexpr auto ref_impls = [] {
        auto ref_targs    = std::vector{^^Trait, ^^Impl};
        auto ref_targs_c  = std::vector{add_const(^^Trait), ^^Impl};
        auto ref_targs_v  = std::vector{add_volatile(^^Trait), ^^Impl};
        auto ref_targs_cv = std::vector{add_cv(^^Trait), ^^Impl};

        auto methods    = std::vector<meta::info>{};
        auto methods_c  = std::vector<meta::info>{};
        auto methods_v  = std::vector<meta::info>{};
        auto methods_cv = std::vector<meta::info>{};
        for (auto grp: trait_method_groups<Trait>) {
            auto const id          = grp.name;
            auto const raw_methods = all_trait_methods<Trait>     //
                                     | stdv::take(grp.end_idx)    //
                                     | stdv::drop(grp.begin_idx);
            auto const raw_impls = full_impls_for<Impl, Trait>    //
                                   | stdv::take(grp.end_idx)      //
                                   | stdv::drop(grp.begin_idx);
            for (auto [i, mem, impl_m]: stdv::zip(stdv::iota(grp.begin_idx), raw_methods, raw_impls)) {
                auto const quals = extract_method_qualifiers(mem);
                auto const spec  = substitute(^^cvts_overload_spec,
                                              {^^Impl,    //
                                               meta::reflect_constant(impl_m.fn),
                                               mem});
                methods.push_back(spec);
                if (quals.is_const)
                    methods_c.push_back(spec);
                if (quals.is_volatile)
                    methods_v.push_back(spec);
                if (quals.is_const and quals.is_volatile)
                    methods_cv.push_back(spec);
            };

            auto const id_refl   = meta::reflect_constant(id);
            auto const maybe_add = [=](auto& targs, auto ref, auto const& methods) {
                if (std::empty(methods))
                    return;
                targs.push_back(
                    substitute(^^cvts_holder, {id_refl, ref, substitute(^^ovspec_holder, methods)}));
            };
            maybe_add(ref_targs, ^^ref, methods);
            maybe_add(ref_targs_c, ^^ref_c, methods_c);
            maybe_add(ref_targs_v, ^^ref_v, methods_v);
            maybe_add(ref_targs_cv, ^^ref_cv, methods_cv);

            methods.clear();
            methods_c.clear();
            methods_v.clear();
            methods_cv.clear();
        }

        return std::array{
            substitute(^^cvts_trait_ref_impl, ref_targs),
            substitute(^^cvts_trait_ref_impl, ref_targs_c),
            substitute(^^cvts_trait_ref_impl, ref_targs_v),
            substitute(^^cvts_trait_ref_impl, ref_targs_cv),
        };
    }();

    using ref_impl_t    = [:ref_impls[0]:];
    using ref_c_impl_t  = [:ref_impls[1]:];
    using ref_v_impl_t  = [:ref_impls[2]:];
    using ref_cv_impl_t = [:ref_impls[3]:];

    struct ref : public ref_impl_t {
        using ref_impl_t::ref_impl_t;
        operator Impl&() const volatile {
            return *extract_obj_ptr(*this);
        }
    };
    struct ref_c : public ref_c_impl_t {
        using ref_c_impl_t::ref_c_impl_t;
        operator Impl&() const volatile {
            return *extract_obj_ptr(*this);
        }
    };
    struct ref_v : public ref_v_impl_t {
        using ref_v_impl_t::ref_v_impl_t;
        operator Impl&() const volatile {
            return *extract_obj_ptr(*this);
        }
    };
    struct ref_cv : public ref_cv_impl_t {
        using ref_cv_impl_t::ref_cv_impl_t;
        operator Impl&() const volatile {
            return *extract_obj_ptr(*this);
        }
    };
};

template<any_trait Trait, implements_trait<Trait>... Impls>
using cvts_trait_ref = [:[] {
    using definer = cvts_ref_definer<std::remove_cvref_t<Trait>, Impls...>;
    if constexpr (std::is_const_v<Trait> and std::is_volatile_v<Trait>) {
        return ^^typename definer::ref_cv;
    } else if constexpr (std::is_volatile_v<Trait>) {
        return ^^typename definer::ref_v;
    } else if constexpr (std::is_const_v<Trait>) {
        return ^^typename definer::ref_c;
    } else {
        return ^^typename definer::ref;
    }
}():];

struct name_info_pair {
    char const* name;
    meta::info  member;
};

template<non_cvref CvtsRef>
inline constexpr auto all_cvref_methods = [] {
    auto mems       = std::vector<name_info_pair>{};
    auto types      = std::vector<meta::info>{^^CvtsRef};
    auto next_types = std::vector<meta::info>{};
    while (not stdr::empty(types)) {
        for (auto t: types) {
            mems.append_range(nonstatic_data_members_of(dealias(t), meta::access_context::unprivileged()) |
                              stdv::transform([](meta::info m) {
                                  return name_info_pair{std::define_static_string(identifier_of(m)), m};
                              }));
            next_types.append_range(subextract_base_types(t));
        }
        swap(types, next_types);
        next_types.clear();
    }
    return std::define_static_array(mems);
}();

template<char const* MethodId, non_cvref CvtsRef>
inline constexpr auto ref_method = [] -> meta::info {
    return stdr::find(all_cvref_methods<CvtsRef>,
                      std::string_view(MethodId),
                      [](auto p) { return std::string_view(p.name); })
        ->member;
}();

template<char const* MethodId, typename... Args>
auto call_method_via_id(auto&& ref, Args&&... args) -> decltype(auto) {
    using ref_t = std::remove_cvref_t<decltype(ref)>;
    return ref.[:ref_method<MethodId, ref_t>:](std::forward<Args>(args)...);
}
}    // namespace cvts_trait


// cv-transient static trait reference
template<any_trait Trait, implements_trait<Trait> Impl>
using cvts_trait_ref = cvts_trait::cvts_trait_ref<Trait, Impl>;
}    // namespace trp::detail
