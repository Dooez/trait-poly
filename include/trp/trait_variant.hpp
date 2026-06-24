#pragma once
#ifndef TRP_GODBOLT
#include "detail/cvts_trait_ref.hpp"
#endif
#include <type_traits>
namespace trp {

namespace detail::var {


template<typename... Impl>
struct impl_holder {
    struct empty {};

    static constexpr auto move_constructible          = (... and std::is_move_constructible_v<Impl>);
    static constexpr auto copy_constructible          = (... and std::is_copy_constructible_v<Impl>);
    static constexpr auto noexcept_copy_constructible = (... and std::is_nothrow_copy_constructible_v<Impl>);
    static constexpr auto noexcept_move_constructible = (... and std::is_nothrow_move_constructible_v<Impl>);

    /**
     * @brief While P3074 is not implemented, we need a wrapper to handle types
     * with nontrivial destructors in undefined unions
     */
    template<typename T>
    union uninit {
        alignas(T) std::array<std::byte, sizeof(T)> storage;

        auto ptr(this auto&& self) {
            using this_t    = std::remove_reference_t<decltype(self)>;
            using value_ptr = [:add_pointer(copy_cv_to(^^this_t, ^^T)):];
            return std::launder(reinterpret_cast<value_ptr>(self.storage.data()));
        }
    };
    union impls_union;
    consteval {
        define_aggregate(^^impls_union,
                         {data_member_spec(^^empty, {.name = "empty"}), anon_member_spec(^^uninit<Impl>)...});
    }
    using tag_t = [:[] {
        constexpr auto n = sizeof...(Impl);
        if (n < std::numeric_limits<u8>::max())
            return ^^u8;
        if (n < std::numeric_limits<u16>::max())
            return ^^u16;
        if (n < std::numeric_limits<u32>::max())
            return ^^u32;
        return ^^u64;
    }():];

    static constexpr tag_t invalid_tag = std::numeric_limits<tag_t>::max();

    impls_union storage{.empty = {}};
    tag_t       tag{invalid_tag};

    static constexpr uZ   count      = sizeof...(Impl);
    static constexpr auto type_infos = std::array{^^Impl...};
    static constexpr auto member_infos =
        std::define_static_array(nonstatic_data_members_of(^^impls_union, unprivileged) | stdv::drop(1));

    template<typename T>
    static constexpr uZ type_count = stdr::count(type_infos, ^^T);

    template<typename T>
    static constexpr uZ type_index = stdr::distance(type_infos.begin(), stdr::find(type_infos, ^^T));

    template<uZ I, typename... Args>
    void construct(constant_wrapper<I>, Args&&... args) {
        std::construct_at(storage.[:member_infos[I]:].ptr(), std::forward<Args>(args)...);
        tag = I;
    };

    template<uZ I>
        requires(I < count)
    void destroy(constant_wrapper<I>) {
        std::destroy_at(storage.[:member_infos[I]:].ptr());
        tag = invalid_tag;
    }

    template<uZ I>
    auto get(this auto&& self, constant_wrapper<I>) -> auto&& {
        using this_t = decltype(self);
        static_assert(not is_rvalue_reference_type(^^this_t));
        return std::forward_like<decltype(self)>(*self.storage.[:member_infos[I]:].ptr());
    }
};

inline constexpr struct {
} obj_union_anno;
template<non_cvref Var>
inline constexpr auto union_member_info = [] {
    auto m = find_annotated_member(^^Var, obj_union_anno, meta::access_context::unchecked());
    if (m == meta::info{})
        throw "No variant union annotated member found";
    return m;
}();
template<typename Union, uZ I>
inline constexpr auto member_type_info = Union::type_infos[I];

constexpr auto extract_union_member(auto&& variant) -> auto&&
    requires(union_member_info<std::remove_cvref_t<decltype(variant)>> != meta::info{})
{
    return std::forward_like<decltype(variant)>(
        variant.[:union_member_info<std::remove_cvref_t<decltype(variant)>>:]);
}
consteval auto extract_var_type_info(meta::info variant, uZ i) -> meta::info {
    auto const mem_inf = extract<meta::info>(substitute(^^union_member_info, {variant}));
    return extract<meta::info>(substitute(^^member_type_info, {type_of(mem_inf), meta::reflect_constant(i)}));
}

template<non_cvref Variant, non_cv_trait Trait, trait_method_idt MethodIdt>
struct cvo_spec {
    using method_idt = MethodIdt;
};

template<non_cvref MethodHolder, non_cvref MethodInvoker, non_cvref OvSpec>
struct cvo_invoker;

#ifdef __cpp_lib_is_pointer_interconvertible
#define TRP_ASSERT_INTERCONVERTIBLE \
    static_assert(std::is_pointer_interconvertible_with_class<MethodHolder>(invoker_ptr));
#else
#define TRP_ASSERT_INTERCONVERTIBLE
#endif

#define TRP_CV_OVERLOAD(Req, C, V)                                                                  \
    template<non_cvref           MethodHolder,                                                      \
             non_cvref           MethodInvoker,                                                     \
             non_cvref           Variant,                                                           \
             non_cv_trait        Trait,                                                             \
             char const*         Id,                                                                \
             method_qualifiers_t Quals,                                                             \
             typename Ret,                                                                          \
             typename... Args>                                                                      \
        requires(Req)                                                                               \
    struct cvo_invoker<MethodHolder,                                                                \
                       MethodInvoker,                                                               \
                       cvo_spec<Variant, Trait, method_identity_t<Id, Quals, Ret, Args...>>> {      \
        auto operator()(Args... args) C V noexcept(Quals.is_noexcept) -> Ret {                      \
            using impl_holder          = std::remove_cvref_t<decltype(get_union_ref())>;            \
            auto const            tag  = get_union_ref().tag;                                       \
            static constexpr auto idxs = make_cw_idxs<impl_holder::count>();                        \
            template for (constexpr auto i: idxs) {                                                 \
                if (tag == i) {                                                                     \
                    using this_t = std::remove_reference_t<decltype(*this)>;                        \
                    /* Use cv-qualified active type because cvts_ref constructs from reference */   \
                    using active_t = [:copy_cv_to(^^this_t, extract_var_type_info(^^Variant, i)):]; \
                    /* Use cv-qualified trait type because cv-qualified active type */              \
                    /* may not implement unqualified trait */                                       \
                    using trait_t  = [:copy_cv_to(^^this_t, ^^Trait):];                             \
                    using cvts_ref = [:copy_cv_to(^^this_t, ^^cvts_trait_ref<trait_t, active_t>):]; \
                                                                                                    \
                    return cvts_trait::call_method_via_id<Id>(cvts_ref(get_union_ref().get(i)),     \
                                                              std::forward<Args>(args)...);         \
                }                                                                                   \
            }                                                                                       \
            if constexpr (Quals.is_noexcept)                                                        \
                std::unreachable();                                                                 \
            else                                                                                    \
                throw std::bad_variant_access{};                                                    \
        }                                                                                           \
                                                                                                    \
    private:                                                                                        \
        auto get_union_ref(this auto&& self) -> auto&& {                                            \
            constexpr auto add_cvp = [](meta::info type) {                                          \
                using this_t = std::remove_reference_t<decltype(self)>;                             \
                return add_pointer(copy_cv_to(^^this_t, type));                                     \
            };                                                                                      \
            static_assert(std::derived_from<MethodInvoker, cvo_invoker>);                           \
            auto const mi_ptr = static_cast<[:add_cvp(^^MethodInvoker):]>(&self);                   \
                                                                                                    \
            constexpr auto invoker_ptr = [] {                                                       \
                auto mems = nonstatic_data_members_of(^^MethodHolder, unprivileged);                \
                if (stdr::size(mems) != 1)                                                          \
                    throw "Method holder is expected to have only a single method.";                \
                if (type_of(mems[0]) != ^^MethodInvoker)                                            \
                    throw "Method invoker type does not match method holders first member type.";   \
                return extract<MethodInvoker MethodHolder::*>(mems[0]);                             \
            }();                                                                                    \
                                                                                                    \
            static_assert(std::is_standard_layout_v<MethodHolder>);                                 \
            TRP_ASSERT_INTERCONVERTIBLE                                                             \
            auto const mh_ptr = reinterpret_cast<[:add_cvp(^^MethodHolder):]>(mi_ptr);              \
                                                                                                    \
            static_assert(std::derived_from<Variant, MethodHolder>);                                \
            return extract_union_member(*static_cast<[:add_cvp(^^Variant):]>(mh_ptr));              \
        }                                                                                           \
    };

TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile, , );
TRP_CV_OVERLOAD(Quals.is_const and not Quals.is_volatile, const, );
TRP_CV_OVERLOAD(not Quals.is_const and Quals.is_volatile, , volatile);
TRP_CV_OVERLOAD(Quals.is_const and Quals.is_volatile, const, volatile);
#undef TRP_CV_OVERLOAD
#undef TRP_ASSERT_INTERCONVERTIBLE

template<non_cvref MethodHolder, non_cvref... OvSpecs>
struct cvm_invoker : public cvo_invoker<MethodHolder, cvm_invoker<MethodHolder, OvSpecs...>, OvSpecs>... {
    using cvo_invoker<MethodHolder, cvm_invoker<MethodHolder, OvSpecs...>, OvSpecs>::operator()...;
};

template<non_cvref... OvSpecs>
struct method_holder_definer {
    struct method_holder;
    consteval {
        using invoker_t = cvm_invoker<method_holder, OvSpecs...>;
        define_aggregate(^^method_holder,
                         {data_member_spec(^^invoker_t,
                                           {
                                               .name              = OvSpecs...[0] ::method_idt::identifier,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};
template<non_cvref... OvSpecs>
inline constexpr auto method_holder_info = ^^typename method_holder_definer<OvSpecs...>::method_holder;

template<typename T>
struct alternative_union_of {
    static constexpr auto value = meta::info{};
};
template<typename T>
concept any_trait_variant =
    non_cvref<T> and std::is_class_v<T> and alternative_union_of<T>::value != meta::info{};
template<typename T>
concept any_trait_variant_cvref = any_trait_variant<std::remove_cvref_t<T>>;
template<typename T, typename Var>
concept unique_alternative_of =
    any_trait_variant<Var> and ([:alternative_union_of<Var>::value:] ::template type_count<T> == 1);

template<non_cvref ImplHolder, non_cvref... MethodHolders>
class trait_variant_impl : public MethodHolders... {
    [[= obj_union_anno]] ImplHolder _;

    [[nodiscard]] constexpr friend auto trait_variant_index(trait_variant_impl const& var) noexcept
        -> ImplHolder::tag_t {
        return extract_union_member(var).tag;
    }
    [[nodiscard]] constexpr friend auto
    trait_variant_valueless_by_exception(trait_variant_impl const& var) noexcept -> bool {
        return extract_union_member(var).tag == ImplHolder::invalid_tag;
    }
    template<uZ I, typename... Args>
    constexpr friend auto trait_variant_emplace(trait_variant_impl& var, Args&&... args) -> auto&& {
        auto&                 union_ref = extract_union_member(var);
        static constexpr auto idxs      = make_cw_idxs<ImplHolder::count>();
        template for (constexpr auto i: idxs) {
            if (union_ref.tag == i)
                union_ref.destroy(i);
        }
        union_ref.construct(cw<I>, std::forward<Args>(args)...);
        return union_ref.get(cw<I>);
    }
    template<uZ I, any_trait_variant_cvref Variant>
        requires(I < ImplHolder::count)
    [[nodiscard]] constexpr friend auto trait_variant_get(Variant&& var) -> decltype(auto) {
        auto&& union_ref = extract_union_member(var);
        if (union_ref.tag != I)
            throw std::bad_variant_access{};
        return std::forward_like<decltype(var)>(union_ref.get(cw<I>));
    }

    template<uZ I, any_trait_variant_cvref Variant>
        requires(I < ImplHolder::count)
    [[nodiscard]] constexpr friend auto trait_variant_get_if(Variant* var) noexcept {
        using pointer_t = decltype(std::addressof(extract_union_member(*var).get(cw<I>)));
        if (var == nullptr)
            return pointer_t{};
        auto&& union_ref = extract_union_member(*var);
        if (union_ref.tag != I)
            return pointer_t{};
        return std::addressof(union_ref.get(cw<I>));
    }

public:
    ~trait_variant_impl() {
        auto&                 union_ref = extract_union_member(*this);
        static constexpr auto idxs      = make_cw_idxs<ImplHolder::count>();
        template for (constexpr auto i: idxs) {
            if (union_ref.tag == i)
                union_ref.destroy(i);
        }
    }
    constexpr explicit trait_variant_impl(trait_variant_impl const& other) noexcept(
        ImplHolder::noexcept_copy_constructible)
        requires(ImplHolder::copy_constructible)
    {
        auto&                 this_union  = extract_union_member(*this);
        auto&&                other_union = extract_union_member(other);
        static constexpr auto idxs        = make_cw_idxs<ImplHolder::count>();
        template for (constexpr auto i: idxs) {
            if (other_union.tag == i)
                this_union.construct(i, other_union.get(i));
        }
    }

    constexpr explicit trait_variant_impl(trait_variant_impl&& other) noexcept(
        ImplHolder::noexcept_move_constructible)
        requires(ImplHolder::move_constructible)
    {
        auto&                 this_union  = extract_union_member(*this);
        auto&                 other_union = extract_union_member(other);
        static constexpr auto idxs        = make_cw_idxs<ImplHolder::count>();
        template for (constexpr auto i: idxs) {
            if (other_union.tag == i)
                this_union.construct(i, std::move(other_union.get(i)));
        }
    }

    template<typename Impl>
        requires(ImplHolder::template type_count<Impl> == 1)
    constexpr explicit trait_variant_impl(Impl value) noexcept(std::is_nothrow_move_constructible_v<Impl>) {
        constexpr auto index = ImplHolder::template type_index<Impl>;
        extract_union_member(*this).construct(cw<index>, std::move(value));
    }

    template<typename Impl, typename... Args>
        requires(ImplHolder::template type_count<Impl> == 1)
    constexpr trait_variant_impl(std::in_place_type_t<Impl>,
                                 Args&&... args) noexcept(std::is_nothrow_constructible_v<Impl, Args...>) {
        constexpr auto index = ImplHolder::template type_index<Impl>;
        extract_union_member(*this).construct(cw<index>, std::forward<Args>(args)...);
    }

    template<uZ I, typename... Args>
        requires(I < ImplHolder::count)
    constexpr trait_variant_impl(std::in_place_index_t<I>, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<typename[:ImplHolder::type_infos[I]:], Args...>) {
        extract_union_member(*this).construct(cw<I>, std::forward<Args>(args)...);
    }

    template<typename Impl>
        requires(ImplHolder::template type_count<std::remove_cvref_t<Impl>> == 1)
    constexpr trait_variant_impl& operator=(Impl&& other) {
        using impl_t             = std::remove_cvref_t<Impl>;
        constexpr auto index     = cw<ImplHolder::template type_index<impl_t>>;
        auto&&         union_ref = extract_union_member(*this);
        auto const     tag       = union_ref.tag;
        if (tag == index) {
            union_ref.get(index) = std::forward<Impl>(other);
            return *this;
        }
        static constexpr auto idxs = make_cw_idxs<ImplHolder::count>();
        template for (constexpr auto i: idxs) {
            if (tag == i)
                union_ref.destroy(i);
        }
        union_ref.construct(index, std::forward<Impl>(other));
        return *this;
    }
};
template<non_cvref ImplHolder, non_cvref... MethodHolders>
auto get_var_impl_holder(trait_variant_impl<ImplHolder, MethodHolders...> const&) -> ImplHolder;

template<non_cv_trait Trait, non_ref... Impls>
struct var_definer {
    struct var;
    struct var_c;
    struct var_v;
    struct var_cv;

    static constexpr auto impls = [] {
        using impl_holder_t = impl_holder<Impls...>;
        auto var_targs      = std::vector{^^impl_holder_t};
        auto var_targs_c    = std::vector{^^impl_holder_t};
        auto var_targs_v    = std::vector{^^impl_holder_t};
        auto var_targs_cv   = std::vector{^^impl_holder_t};

        auto holder_targs    = std::vector<meta::info>{};
        auto holder_targs_c  = std::vector<meta::info>{};
        auto holder_targs_v  = std::vector<meta::info>{};
        auto holder_targs_cv = std::vector<meta::info>{};

        for (auto const grp: trait_method_groups<Trait>) {
            auto const raw_methods = all_trait_methods<Trait>     //
                                     | stdv::take(grp.end_idx)    //
                                     | stdv::drop(grp.begin_idx);
            for (auto mem: raw_methods) {
                auto const quals     = extract_method_qualifiers(mem);
                auto const maybe_add = [=](auto& targs, meta::info var_info, bool do_add) {
                    if (not do_add)
                        return;
                    auto const spec = substitute(^^cvo_spec, {var_info, ^^Trait, mem});
                    targs.push_back(spec);
                };
                maybe_add(holder_targs, ^^var, true);
                maybe_add(holder_targs_c, ^^var_c, quals.is_const);
                maybe_add(holder_targs_v, ^^var_v, quals.is_volatile);
                maybe_add(holder_targs_cv, ^^var_cv, quals.is_const and quals.is_volatile);
            };

            auto const add_nonempty = [=](auto& targs, auto& holder_targs) {
                if (stdr::empty(holder_targs))
                    return;
                targs.push_back(extract<meta::info>(substitute(^^method_holder_info, holder_targs)));
                holder_targs.clear();
            };
            add_nonempty(var_targs, holder_targs);
            add_nonempty(var_targs_c, holder_targs_c);
            add_nonempty(var_targs_v, holder_targs_v);
            add_nonempty(var_targs_cv, holder_targs_cv);
        }

        return std::array{
            substitute(^^trait_variant_impl, var_targs),
            substitute(^^trait_variant_impl, var_targs_c),
            substitute(^^trait_variant_impl, var_targs_v),
            substitute(^^trait_variant_impl, var_targs_cv),
        };
    }();
    using impl_t    = [:impls[0]:];
    using impl_c_t  = [:impls[1]:];
    using impl_v_t  = [:impls[2]:];
    using impl_cv_t = [:impls[3]:];

    struct var : public impl_t {
        using impl_t::impl_t;
        using impl_t::operator=;
    };
    struct var_c : public impl_c_t {
        using impl_c_t::impl_c_t;
        using impl_c_t::operator=;
    };
    struct var_v : public impl_v_t {
        using impl_v_t::impl_v_t;
        using impl_v_t::operator=;
    };
    struct var_cv : public impl_cv_t {
        using impl_cv_t::impl_cv_t;
        using impl_cv_t::operator=;
    };
};
template<typename T>
    requires requires(T v) { get_var_impl_holder(v); }
struct alternative_union_of<T> {
    static constexpr auto value = ^^decltype(get_var_impl_holder(std::declval<T>()));
};

template<any_trait Trait, implements_trait<Trait>... Impls>
using trait_variant_alias = [:[] {
    using definer = var_definer<std::remove_cvref_t<Trait>, Impls...>;
    if constexpr (std::is_const_v<Trait> and std::is_volatile_v<Trait>) {
        return ^^typename definer::var_cv;
    } else if constexpr (std::is_volatile_v<Trait>) {
        return ^^typename definer::var_v;
    } else if constexpr (std::is_const_v<Trait>) {
        return ^^typename definer::var_c;
    } else {
        return ^^typename definer::var;
    }
}():];

template<any_trait_variant Var>
inline constexpr auto variant_size = [:alternative_union_of<Var>::value:] ::count;
template<uZ I, any_trait_variant Var>
    requires(I < variant_size<Var>)
using variant_alternative = [:[:alternative_union_of<Var>::value:] ::type_infos[I]:];
template<typename T, any_trait_variant Var>
inline constexpr auto
    variant_alternative_index = [:alternative_union_of<Var>::value:] ::template type_index<T>;

}    // namespace detail::var

template<any_trait Trait, implements_trait<Trait>... Impls>
class trait_variant : public detail::var::trait_variant_alias<Trait, Impls...> {
public:
    using detail::var::trait_variant_alias<Trait, Impls...>::trait_variant_alias;
    using detail::var::trait_variant_alias<Trait, Impls...>::operator=;
};

template<detail::var::any_trait_variant Var>
[[nodiscard]] constexpr auto index(Var const& var) noexcept {
    return trait_variant_index(var);
}

template<detail::var::any_trait_variant Var>
[[nodiscard]] constexpr auto valueless_by_exception(Var const& var) noexcept -> bool {
    return trait_variant_valueless_by_exception(var);
}

template<uZ I, detail::var::any_trait_variant Var, typename... Args>
    requires(I < detail::var::variant_size<Var>)
constexpr auto emplace(Var& var, Args&&... args) -> decltype(auto) {
    return trait_variant_emplace<I>(var, std::forward<Args>(args)...);
}

template<typename T, detail::var::any_trait_variant Var, typename... Args>
    requires detail::var::unique_alternative_of<T, Var>
constexpr auto emplace(Var& var, Args&&... args) -> T& {
    constexpr auto i = detail::var::variant_alternative_index<T, Var>;
    return emplace<i>(var, std::forward<Args>(args)...);
}

template<uZ I, detail::var::any_trait_variant_cvref Variant>
    requires(I < detail::var::variant_size<std::remove_cvref_t<Variant>>)
[[nodiscard]] constexpr auto get(Variant&& var) -> decltype(auto) {
    return trait_variant_get<I>(std::forward<Variant>(var));
}

template<typename T, detail::var::any_trait_variant_cvref Variant>
    requires detail::var::unique_alternative_of<T, std::remove_cvref_t<Variant>>
[[nodiscard]] constexpr auto get(Variant&& var) -> decltype(auto) {
    constexpr auto i = detail::var::variant_alternative_index<T, std::remove_cvref_t<Variant>>;
    return get<i>(std::forward<Variant>(var));
}

template<typename T, detail::var::any_trait_variant Var>
    requires detail::var::unique_alternative_of<T, Var>
[[nodiscard]] constexpr auto holds_alternative(Var const& var) noexcept -> bool {
    constexpr auto i = detail::var::variant_alternative_index<T, Var>;
    return index(var) == i;
}

template<uZ I, detail::var::any_trait_variant_cvref Variant>
    requires(I < detail::var::variant_size<std::remove_cvref_t<Variant>>)
[[nodiscard]] constexpr auto get_if(Variant* var) noexcept {
    return trait_variant_get_if<I>(var);
}

template<typename T, detail::var::any_trait_variant_cvref Variant>
    requires detail::var::unique_alternative_of<T, std::remove_cvref_t<Variant>>
[[nodiscard]] constexpr auto get_if(Variant* var) noexcept {
    constexpr auto i = detail::var::variant_alternative_index<T, std::remove_cvref_t<Variant>>;
    return get_if<i>(var);
}

}    // namespace trp

namespace std {

template<typename Var>
    requires trp::detail::var::any_trait_variant<Var>
struct variant_size<Var> : std::integral_constant<std::size_t, trp::detail::var::variant_size<Var>> {};

template<std::size_t I, typename Var>
    requires trp::detail::var::any_trait_variant<Var> and (I < trp::detail::var::variant_size<Var>)
struct variant_alternative<I, Var> {
    using type = trp::detail::var::variant_alternative<I, Var>;
};

}    // namespace std
