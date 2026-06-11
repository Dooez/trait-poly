#pragma once
#ifndef TRP_GODBOLT
#include "detail/cvts_trait_ref.hpp"
#endif

#include <type_traits>
namespace trp {

namespace detail {

namespace var {

template<typename... Impl>
struct impl_union_definer {
    struct empty {};
    union impls_union;
    consteval {
        define_aggregate(^^impls_union,
                         {data_member_spec(^^empty, {.name = "empty"}), anon_member_spec(^^Impl)...});
    }
    using tag_t = [:[] {
        constexpr auto n = sizeof...(Impl);
        if (n <= std::numeric_limits<u8>::max())
            return ^^u8;
        if (n <= std::numeric_limits<u16>::max())
            return ^^u16;
        if (n <= std::numeric_limits<u32>::max())
            return ^^u32;
        return ^^u64;
    }():];

    struct impls {
        impls_union storage{.empty = {}};
        tag_t       tag;

        static constexpr uZ   count        = sizeof...(Impl);
        static constexpr auto type_infos   = std::array{^^Impl...};
        static constexpr auto member_infos = std::define_static_array(
            nonstatic_data_members_of(^^impls_union, std::meta::access_context::current()));

        template<typename T>
        static constexpr auto type_count = stdr::count(type_infos, ^^T);

        template<typename T>
        static constexpr auto type_index = stdr::distance(type_infos.begin(), stdr::find(type_infos, ^^T));

        template<uZ I, typename... Args>
        void construct(Args&&... args) {
            std::construct_at(&storage.[:member_infos[I + 1]:], std::forward<Args>(args)...);
            tag = I;
        };
        template<uZ I>
        void destroy() {
            std::destroy_at(&storage.[:member_infos[I + 1]:]);
        }
        template<uZ I>
        auto get(this auto&& self) -> decltype(auto) {
            return self.storage.[:member_infos[I + 1]:];
        }
    };
};
inline constexpr struct {
} obj_union_anno;
template<non_cvref Var>
inline constexpr auto union_member_info = [] {
    auto m = find_annotated_member(^^Var, ^^obj_union_anno);
    if (m == meta::info{})
        throw "No vtable annotated member found";
    return m;
}();
template<typename Union, uZ I>
inline constexpr auto member_type_info = Union::member_infos[I + 1];

auto extract_union_member(auto&& variant) -> auto&&
    requires(union_member_info<std::remove_cvref_t<decltype(variant)>> != meta::info{})
{
    return variant.[:union_member_info<std::remove_cvref_t<decltype(variant)>>:];
}
consteval auto extract_var_type_info(meta::info variant, uZ i) -> meta::info {
    auto const mem_inf = extract<meta::info>(substitute(^^union_member_info, {variant}));
    return extract<meta::info>(substitute(^^member_type_info, {type_of(mem_inf), meta::reflect_constant(i)}));
}


template<meta::info... Methods>
struct method_pack {
    struct m_id {
        uZ         index;
        meta::info info;
    };

    static constexpr auto methods = [] {
        auto [... Is] = make_cw_idxs<sizeof...(Methods)>();
        return make_aggregate(m_id{Is, Methods}...);
    }();
};

template<typename Var, typename Trait, typename MethodPack, trait_method_idt MethodIdt>
struct overload_spec {};

template<typename MethodInvoker, typename MethodHolder, typename OvSpec>
struct cvo_invoker;
template<typename MethodInvoker,
         typename MethodHolder,
         typename Var,
         typename Trait,
         typename MethodPack,
         auto                Identifier,
         method_qualifiers_t Quals,
         typename Ret,
         typename... Args>
struct cvo_invoker<
    MethodInvoker,
    MethodHolder,
    overload_spec<Var, Trait, MethodPack, method_identity_t<Identifier, Quals, Ret, Args...>>> {
    auto operator()(Args... args) noexcept(Quals.is_noexcept) -> Ret
        requires(not Quals.is_const and not Quals.is_volatile)
    {
        auto const tag = extract_union_member(get_var_ref()).tag;

        template for (constexpr auto m: MethodPack::methods) {
            if (tag == m.index)
                return invoke_method<m.index, m.info>(std::forward<Args>(args)...);
        }
    }
    // auto operator()(Args... args) const noexcept(Quals.is_noexcept) -> Ret
    //     requires(Quals.is_const and not Quals.is_volatile)
    // {
    //     auto const tag = extract_union_member(get_var_ref()).tag;
    //
    //     template for (constexpr auto m: MethodPack::methods) {
    //         if (tag == m.index)
    //             return invoke_method<m.index, m.info>(std::forward<Args>(args)...);
    //     }
    // }
    // auto operator()(Args... args) volatile noexcept(Quals.is_noexcept) -> Ret
    //     requires(not Quals.is_const and Quals.is_volatile)
    // {
    //     auto const tag = extract_union_member(get_var_ref()).tag;
    //
    //     template for (constexpr auto m: MethodPack::methods) {
    //         if (tag == m.index)
    //             return invoke_method<m.index, m.info>(std::forward<Args>(args)...);
    //     }
    // }
    // auto operator()(Args... args) const volatile noexcept(Quals.is_noexcept) -> Ret
    //     requires(Quals.is_const and Quals.is_volatile)
    // {
    //     auto const tag = extract_union_member(get_var_ref()).tag;
    //
    //     template for (constexpr auto m: MethodPack::methods) {
    //         if (tag == m.index)
    //             return invoke_method<m.index, m.info>(std::forward<Args>(args)...);
    //     }
    // }

private:
    template<uZ I, meta::info Method>
    auto invoke_method(this auto&& self, Args&&... args) -> Ret {
        if constexpr (is_template(Method)) {
            using this_t     = std::remove_reference_t<decltype(self)>;
            using active_t   = [:extract_var_type_info(^^Var, I):];
            using active_ref = [:copy_cv_to(^^this_t, ^^active_t):];
            using cvts_ref   = cvts_trait_ref<Trait, active_t>;

            constexpr auto explicit_method = substitute(Method, {^^cvts_ref});
            return [:explicit_method:](extract_union_member(self.get_var_ref()).template get<I>(),
                                       std::forward<Args>(args)...);
        } else {
            return extract_union_member(self.get_var_ref()).template get<I>().[:Method:](
                std::forward<Args>(args)...);
        }
    }

    auto get_var_ref(this auto&& self) -> decltype(auto) {
        constexpr auto add_cvp = [](meta::info type) {
            using this_t = std::remove_reference_t<decltype(self)>;
            return add_pointer(copy_cv_to(^^this_t, type));
        };

        static_assert(std::derived_from<MethodInvoker, cvo_invoker>);
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

        static_assert(std::derived_from<Var, MethodHolder>);
        return *static_cast<[:add_cvp(^^Var):]>(mh_ptr);
    }
};
template<typename MethodHolder, typename... OvSpecs>
struct cvm_invoker : cvo_invoker<cvm_invoker<MethodHolder, OvSpecs...>, MethodHolder, OvSpecs>... {
    using cvo_invoker<cvm_invoker<MethodHolder, OvSpecs...>, MethodHolder, OvSpecs>::operator()...;
};


template<char const* id, typename... OvSpecs>
struct method_holder_definer {
    struct method_holder;
    consteval {
        using invoker_t = cvm_invoker<method_holder, OvSpecs...>;
        define_aggregate(^^method_holder,
                         {data_member_spec(^^invoker_t,
                                           {
                                               .name              = id,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};
template<char const* id, typename... OvSpecs>
inline constexpr auto method_holder_info = ^^typename method_holder_definer<id, OvSpecs...>::method_holder;

template<typename Trait, typename ImplHolder, typename... MethodHolders>
class trait_variant_impl : public MethodHolders... {
    [[= obj_union_anno]] ImplHolder _;

public:
    template<typename Impl>
        requires(ImplHolder::template type_count<Impl> == 1)
    constexpr explicit trait_variant_impl(Impl value) noexcept(std::is_nothrow_move_constructible_v<Impl>) {
        constexpr auto index = ImplHolder::template type_index<Impl>;
        extract_union_member(*this).template construct<index>(std::move(value));
    }
    template<typename Impl, typename... Args>
        requires(ImplHolder::template type_count<Impl> == 1)
    constexpr trait_variant_impl(std::in_place_type_t<Impl>,
                                 Args&&... args) noexcept(std::is_nothrow_constructible_v<Impl, Args...>) {
        constexpr auto index = ImplHolder::template type_index<Impl>;
        extract_union_member(*this).template construct<index>(std::forward<Args>(args)...);
    }
    template<uZ I, typename... Args>
        requires(I < ImplHolder::count)
    constexpr trait_variant_impl(std::in_place_index_t<I>, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<typename[:ImplHolder::type_infos[I]:], Args...>) {
        extract_union_member(*this).template construct<I>(std::forward<Args>(args)...);
    }

    template<typename Impl>
        requires(ImplHolder::template type_count<std::remove_cvref_t<Impl>> == 1)
    constexpr trait_variant_impl& operator=(Impl&& other) {
        using impl_t             = std::remove_cvref_t<Impl>;
        constexpr auto index     = ImplHolder::template type_index<impl_t>;
        auto&&         union_ref = extract_union_member(*this);
        auto const     tag       = union_ref.tag;
        if (tag == index) {
            union_ref.template get<index>() = std::forward<Impl>(other);
            return *this;
        }
        template for (constexpr auto i: stdv::iota(0UZ, ImplHolder::count)) {
            if (tag == i)
                union_ref.template destroy<i>();
        }
        union_ref.template construct<index>(std::forward<Impl>(other));
        return *this;
    }


    // template<typename Impl>
    //     requires(ImplHolder::template type_count<Impl> == 1)
    // constexpr explicit trait_variant_impl(Impl value) {
    //     extract_union_member(*this).template construct<ImplHolder::template type_index<Impl>>(
    //         std::move(value));
    // }
};


template<non_cv_trait Trait, implements_trait<Trait>... Impls>
struct var_wrappers {
    struct var;
    // struct var_c;
    // struct var_v;
    // struct var_cv;

    static constexpr auto impls = [] {
        using impl_holder_t = impl_union_definer<Impls...>::impls;
        auto var_targs      = std::vector{^^Trait, ^^impl_holder_t};
        // auto ref_targs_c  = std::vector{add_const(^^Trait)};
        // auto ref_targs_v  = std::vector{add_volatile(^^Trait)};
        // auto ref_targs_cv = std::vector{add_cv(^^Trait)};

        auto holder_targs = std::vector<meta::info>{};
        // bool is_c    = false;
        // bool is_v    = false;
        // bool is_cv   = false;
        for (auto grp: trait_method_groups<Trait>) {
            auto const id      = grp.name;
            auto const id_refl = meta::reflect_constant(id);
            holder_targs.push_back(id_refl);
            auto const raw_methods = all_trait_methods<Trait>     //
                                     | stdv::take(grp.end_idx)    //
                                     | stdv::drop(grp.begin_idx);
            auto const [... raw_impls] = make_aggregate(full_impls_for<Impls, Trait>    //
                                                        | stdv::take(grp.end_idx)       //
                                                        | stdv::drop(grp.begin_idx)...);
            for (auto [mem, ... impls]: stdv::zip(raw_methods, raw_impls...)) {
                auto const quals            = extract_method_qualifiers(mem);
                auto const method_pack_vals = std::vector<meta::info>{};
                auto const method_pack_info =
                    substitute(^^method_pack, {meta::reflect_constant(impls.fn)...});
                auto const spec = substitute(^^overload_spec, {^^var, ^^Trait, method_pack_info, mem});
                holder_targs.push_back(spec);
                // is_c  = is_c or quals.is_const;
                // is_v  = is_v or quals.is_volatile;
                // is_cv = is_cv or (quals.is_const and quals.is_volatile);
            };

            auto const maybe_add = [=](auto& targs, auto ref, auto trait, bool do_add) {
                if (not do_add)
                    return;
                targs.push_back(extract<meta::info>(substitute(^^method_holder_info, holder_targs)));
            };
            maybe_add(var_targs, ^^var, ^^Trait, true);
            // maybe_add(ref_targs_c, ^^var_c, add_const(^^Trait), is_c);
            // maybe_add(ref_targs_v, ^^var_v, add_volatile(^^Trait), is_v);
            // maybe_add(ref_targs_cv, ^^var_cv, add_cv(^^Trait), is_cv);

            holder_targs.clear();
        }

        return std::array{
            substitute(^^trait_variant_impl, var_targs),
            substitute(^^trait_variant_impl, var_targs),
            substitute(^^trait_variant_impl, var_targs),
            substitute(^^trait_variant_impl, var_targs),
            // substitute(^^trait_variant_impl, ref_targs_c),
            // substitute(^^trait_variant_impl, ref_targs_v),
            // substitute(^^trait_variant_impl, ref_targs_cv),
        };
    }();
    using impl_t = [:impls[0]:];
    // using impl_c_t  = [:impls[1]:];
    // using impl_v_t  = [:impls[2]:];
    // using impl_cv_t = [:impls[3]:];

    struct var : public impl_t {
        using impl_t::impl_t;
        using impl_t::operator=;
    };
    // struct ref_c : public impl_c_t {
    //     using impl_c_t::impl_c_t;
    // };
    // struct ref_v : public impl_v_t {
    //     using impl_v_t::impl_v_t;
    // };
    // struct ref_cv : public impl_cv_t {
    //     using impl_cv_t::impl_cv_t;
    // };
};

}    // namespace var
}    // namespace detail

template<non_cv_trait Trait, implements_trait<Trait>... Impls>
using trait_variant = detail::var::var_wrappers<Trait, Impls...>::var;

}    // namespace trp
