#pragma once
#ifndef TRP_GODBOLT
#include "detail/cvts_trait_ref.hpp"
#endif
#include <exception>
#include <type_traits>
namespace trp {

namespace detail::var {

/**
 * @brief While P3074 is not implemented, we need a wrapper to handle types
 * with nontrivial destructors in undefined unions
 */
template<typename T>
union uninit {
    alignas(T) std::array<std::byte, sizeof(T)> storage;

    auto ptr(this auto&& self) {
        using this_t     = std::remove_reference_t<decltype(self)>;
        using value_ptr  = [:add_pointer(copy_cv_to(^^this_t, ^^T)):];
        using unvolatile = [:remove_volatile(^^this_t):];
        return std::launder(reinterpret_cast<value_ptr>(const_cast<unvolatile&>(self).storage.data()));
    }
};

template<typename... Impl>
struct impl_holder {
    struct empty {};

    static constexpr auto move_constructible          = (... and std::is_move_constructible_v<Impl>);
    static constexpr auto copy_constructible          = (... and std::is_copy_constructible_v<Impl>);
    static constexpr auto noexcept_copy_constructible = (... and std::is_nothrow_copy_constructible_v<Impl>);
    static constexpr auto noexcept_move_constructible = (... and std::is_nothrow_move_constructible_v<Impl>);

    union impls_union;
    consteval {
        define_aggregate(^^impls_union,
                         {data_member_spec(^^empty, {.name = "empty"}), anon_member_spec(^^uninit<Impl>)...});
    }
    using tag_t =    //
      [:sizeof...(Impl) < std::numeric_limits<u8>::max()    ? ^^u8
        : sizeof...(Impl) < std::numeric_limits<u16>::max() ? ^^u16
        : sizeof...(Impl) < std::numeric_limits<u32>::max() ? ^^u32
                                                            : ^^u64:];

    static constexpr tag_t invalid_tag = std::numeric_limits<tag_t>::max();

    impls_union storage{.empty = {}};
    tag_t       tag{invalid_tag};

    static constexpr uZ count = sizeof...(Impl);
    template<uZ I>
        requires(I < sizeof...(Impl))
    using type = Impl...[I];

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
inline constexpr auto union_member_info = find_annotated_member(^^Var,    //
                                                                obj_union_anno,
                                                                "No variant union annotated member found",
                                                                meta::access_context::unchecked());
template<typename Union, uZ I>
inline constexpr auto member_type_info = Union::type_infos[I];

constexpr auto extract_union_member(auto&& variant) -> decltype(auto)
    requires(union_member_info<std::remove_cvref_t<decltype(variant)>> != meta::info{})
{
    return std::forward_like<decltype(variant)>(
        variant.[:union_member_info<std::remove_cvref_t<decltype(variant)>>:]);
}
consteval auto extract_var_type_info(meta::info variant, uZ i) -> meta::info {
    auto const mem_inf = extract<meta::info>(substitute(^^union_member_info, {variant}));
    return extract<meta::info>(substitute(^^member_type_info, {type_of(mem_inf), meta::reflect_constant(i)}));
}

template<non_cvref        Variant,
         non_cvref        MethodHolder,
         non_cvref        MethodInvoker,
         non_cv_trait     Trait,
         trait_method_idt MethodIdt>
struct cvo_invoker;

#define TRP_DEV
#ifdef TRP_DEV

#define TRP_ASSERT_DERIVED_INVOKER static_assert(std::derived_from<MethodInvoker, cvo_invoker>);
#define TRP_ASSERT_STANDARD_LAYOUT static_assert(std::is_standard_layout_v<MethodHolder>);
#ifdef __cpp_lib_is_pointer_interconvertible
#define TRP_ASSERT_INTERCONVERTIBLE                                                                       \
    static_assert(                                                                                        \
        std::is_pointer_interconvertible_with_class<MethodHolder>(extract<MethodInvoker MethodHolder::*>( \
            get_first_member_check_type(^^MethodHolder, ^^MethodInvoker))));
#else
#define TRP_ASSERT_INTERCONVERTIBLE
#endif
#define TRP_ASSERT_DERIVED_VARIANT static_assert(std::derived_from<Variant, MethodHolder>);
#else

#define TRP_ASSERT_DERIVED_INVOKER
#define TRP_ASSERT_STANDARD_LAYOUT
#define TRP_ASSERT_INTERCONVERTIBLE
#define TRP_ASSERT_DERIVED_VARIANT

#endif
#define TRP_ADD_CVP(type) add_pointer(copy_cv_to(^^std::remove_pointer_t<decltype(this)>, type))

#define TRP_CV_OVERLOAD(Req, C, V, Ref)                                                                      \
    template<non_cvref           Variant,                                                                    \
             non_cvref           MethodHolder,                                                               \
             non_cvref           MethodInvoker,                                                              \
             non_cv_trait        Trait,                                                                      \
             char const*         Id,                                                                         \
             method_qualifiers_t Quals,                                                                      \
             typename Ret,                                                                                   \
             typename... Args>                                                                               \
        requires(Req)                                                                                        \
    struct cvo_invoker<Variant,                                                                              \
                       MethodHolder,                                                                         \
                       MethodInvoker,                                                                        \
                       Trait,                                                                                \
                       method_identity_t<Id, Quals, Ret, Args...>> {                                         \
        auto operator()(Args... args) C V Ref noexcept(Quals.is_noexcept) -> Ret {                           \
            using this_ref_t           = [:Quals.is_rvalue ? (^^cvo_invoker C V&&) : (^^cvo_invoker C V&):]; \
            decltype(auto) union_ref   = static_cast<this_ref_t>(*this).get_union_ref();                     \
            using impl_holder          = std::remove_cvref_t<decltype(union_ref)>;                           \
            auto const            tag  = union_ref.tag;                                                      \
            static constexpr auto idxs = make_cw_idxs<impl_holder::count>();                                 \
            template for (constexpr auto i: idxs) {                                                          \
                if (tag == i) {                                                                              \
                    /* Use cv-qualified active type because cvts_ref constructs from reference */            \
                    using active_t = [:extract_var_type_info(^^Variant, i):];                                \
                    /* Use cv-qualified trait type because cv-qualified active type */                       \
                    /* may not implement unqualified trait */                                                \
                    using cvts_ref = cvts_trait_ref<Trait C V, active_t C V>;                                \
                                                                                                             \
                    auto ref = cvts_ref(union_ref.get(i));                                                   \
                    if constexpr (Quals.is_rvalue)                                                           \
                        return cvts_trait::call_method_via_id<Id>(std::move(ref),                            \
                                                                  std::forward<Args>(args)...);              \
                    else                                                                                     \
                        return cvts_trait::call_method_via_id<Id>(ref, std::forward<Args>(args)...);         \
                }                                                                                            \
            }                                                                                                \
            if constexpr (Quals.is_noexcept)                                                                 \
                std::terminate();                                                                            \
            else                                                                                             \
                throw std::bad_variant_access{};                                                             \
        }                                                                                                    \
                                                                                                             \
    private:                                                                                                 \
        auto get_union_ref() C V Ref -> decltype(auto) {                                                     \
            TRP_ASSERT_DERIVED_INVOKER                                                                       \
            auto const mi_ptr = static_cast<[:TRP_ADD_CVP(^^MethodInvoker):]>(this);                         \
                                                                                                             \
            TRP_ASSERT_STANDARD_LAYOUT                                                                       \
            TRP_ASSERT_INTERCONVERTIBLE                                                                      \
            auto const mh_ptr = reinterpret_cast<[:TRP_ADD_CVP(^^MethodHolder):]>(mi_ptr);                   \
                                                                                                             \
            TRP_ASSERT_DERIVED_VARIANT                                                                       \
            using out_ref_t = [:Quals.is_rvalue ? (^^Variant C V&&) : (^^Variant C V&):];                    \
            return extract_union_member(                                                                     \
                static_cast<out_ref_t>(*static_cast<[:TRP_ADD_CVP(^^Variant):]>(mh_ptr)));                   \
        }                                                                                                    \
    };

// clang-format off

TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile and not Quals.is_ref(),       ,         ,   );
TRP_CV_OVERLOAD(    Quals.is_const and not Quals.is_volatile and not Quals.is_ref(),  const,         ,   );
TRP_CV_OVERLOAD(not Quals.is_const and     Quals.is_volatile and not Quals.is_ref(),       , volatile,   );
TRP_CV_OVERLOAD(    Quals.is_const and     Quals.is_volatile and not Quals.is_ref(),  const, volatile,   );

TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile and     Quals.is_lvalue,      ,         , & );
TRP_CV_OVERLOAD(    Quals.is_const and not Quals.is_volatile and     Quals.is_lvalue, const,         , & );
TRP_CV_OVERLOAD(not Quals.is_const and     Quals.is_volatile and     Quals.is_lvalue,      , volatile, & );
TRP_CV_OVERLOAD(    Quals.is_const and     Quals.is_volatile and     Quals.is_lvalue, const, volatile, & );

TRP_CV_OVERLOAD(not Quals.is_const and not Quals.is_volatile and     Quals.is_rvalue,      ,         , &&);
TRP_CV_OVERLOAD(    Quals.is_const and not Quals.is_volatile and     Quals.is_rvalue, const,         , &&);
TRP_CV_OVERLOAD(not Quals.is_const and     Quals.is_volatile and     Quals.is_rvalue,      , volatile, &&);
TRP_CV_OVERLOAD(    Quals.is_const and     Quals.is_volatile and     Quals.is_rvalue, const, volatile, &&);

// clang-format on

#undef TRP_ASSERT_DERIVED_TREF
#undef TRP_ASSERT_INTERCONVERTIBLE
#undef TRP_ASSERT_STANDARD_LAYOUT
#undef TRP_ASSERT_DERIVED_INVOKER

#undef TRP_ADD_CVP
#undef TRP_CV_OVERLOAD
#undef TRP_ASSERT_INTERCONVERTIBLE

// template<non_cvref Variantn, non_cvref MethodHolder, non_cvref MethodInvoker, non_cv_trait Trait, trait_method_idt MethodIdt>
// struct cvo_invoker;
template<non_cvref Variant, non_cvref MethodHolder, non_cvref Trait, non_cvref... MethodIdts>
struct cvm_invoker
: public cvo_invoker<Variant,
                     MethodHolder,
                     cvm_invoker<Variant, MethodHolder, Trait, MethodIdts...>,
                     Trait,
                     MethodIdts>... {
    using cvo_invoker<Variant, MethodHolder, cvm_invoker, Trait, MethodIdts>::operator()...;
};

template<non_cvref Variant, non_cvref Trait, non_cvref... MethodIdts>
struct method_holder_definer {
    struct method_holder;
    consteval {
        using invoker_t = cvm_invoker<Variant, method_holder, Trait, MethodIdts...>;
        define_aggregate(^^method_holder,
                         {data_member_spec(^^invoker_t,
                                           {
                                               .name              = MethodIdts...[0] ::identifier,
                                               .no_unique_address = true,
                                               //  .attributes = {^^[[no_unique_address]] },
                                           })});
    }
};

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

consteval auto get_holder_definer(meta::info variant, meta::info trait, meta::info method_idts_refl) {
    auto       targs = std::vector{variant, trait};
    auto const idts =
        std::span(extract<meta::info const*>(method_idts_refl), extent(type_of(method_idts_refl)));
    targs.append_range(idts);
    return substitute(^^method_holder_definer, targs);
}

template<non_cvref ImplHolder, any_trait Trait, meta::info... MethodIdtsRefls>
class trait_variant_impl
: public[:get_holder_definer(^^trait_variant_impl<ImplHolder, Trait, MethodIdtsRefls...>,
                             ^^Trait,
                             MethodIdtsRefls):] ::method_holder... {
    [[= obj_union_anno]] ImplHolder _;

    [[nodiscard]] constexpr friend auto trait_variant_index(trait_variant_impl const& var) noexcept
        -> ImplHolder::tag_t {
        return extract_union_member(var).tag;
    }
    [[nodiscard]] constexpr friend auto trait_variant_valueless_by_exception(
        trait_variant_impl const& var) noexcept -> bool {
        return extract_union_member(var).tag == ImplHolder::invalid_tag;
    }
    template<uZ I, typename... Args>
    constexpr friend auto trait_variant_emplace(trait_variant_impl & var, Args && ... args) -> auto&& {
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
    [[nodiscard]] constexpr friend auto trait_variant_get(Variant && var) -> decltype(auto) {
        auto&& union_ref = extract_union_member(var);
        if (union_ref.tag != I)
            throw std::bad_variant_access{};
        return std::forward_like<decltype(var)>(union_ref.get(cw<I>));
    }

    template<uZ I, any_trait_variant_cvref Variant>
        requires(I < ImplHolder::count)
    [[nodiscard]] constexpr friend auto trait_variant_get_if(Variant * var) noexcept {
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

    constexpr trait_variant_impl(trait_variant_impl const& other) noexcept(
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

    constexpr trait_variant_impl(trait_variant_impl &&
                                 other) noexcept(ImplHolder::noexcept_move_constructible)
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
        requires unique_alternative_of<std::remove_cvref_t<Impl>, trait_variant_impl> and
                 std::is_constructible_v<std::remove_cvref_t<Impl>, Impl&&>
    constexpr explicit trait_variant_impl(Impl && value) noexcept(
        std::is_nothrow_constructible_v<std::remove_cvref_t<Impl>, Impl&&>) {
        using impl_t         = std::remove_cvref_t<Impl>;
        constexpr auto index = ImplHolder::template type_index<impl_t>;
        extract_union_member(*this).construct(cw<index>, std::forward<Impl>(value));
    }

    template<unique_alternative_of<trait_variant_impl> Impl, typename... Args>
        requires(std::is_constructible_v<Impl, Args...>)
    constexpr trait_variant_impl(std::in_place_type_t<Impl>,
                                 Args && ... args) noexcept(std::is_nothrow_constructible_v<Impl, Args...>) {
        constexpr auto index = ImplHolder::template type_index<Impl>;
        extract_union_member(*this).construct(cw<index>, std::forward<Args>(args)...);
    }

    template<uZ I, typename... Args>
        requires(I < ImplHolder::count and
                 std::is_constructible_v<typename[:ImplHolder::type_infos[I]:], Args...>)
    constexpr trait_variant_impl(std::in_place_index_t<I>, Args && ... args) noexcept(
        std::is_nothrow_constructible_v<typename[:ImplHolder::type_infos[I]:], Args...>) {
        extract_union_member(*this).construct(cw<I>, std::forward<Args>(args)...);
    }

    constexpr trait_variant_impl& operator=(trait_variant_impl&& other) noexcept(
        ImplHolder::noexcept_move_constructible)
        requires(ImplHolder::move_constructible)
    {
        if (this == &other)
            return *this;
        auto&                 this_union  = extract_union_member(*this);
        auto&                 other_union = extract_union_member(other);
        static constexpr auto idxs        = make_cw_idxs<ImplHolder::count>();

        template for (constexpr auto i: idxs) {
            if (other_union.tag == i) {
                if constexpr (std::is_move_assignable_v<typename ImplHolder::template type<i>> and
                              (not ImplHolder::noexcept_move_constructible or
                               std::is_nothrow_move_assignable_v<typename ImplHolder::template type<i>>)) {
                    if (this_union.tag == other_union.tag) {
                        this_union.get(i) = std::move(other_union.get(i));
                        return *this;
                    }
                }
                template for (constexpr auto j: idxs) {
                    if (this_union.tag == j)
                        this_union.destroy(j);
                }
                this_union.construct(i, std::move(other_union.get(i)));
                return *this;
            }
        }
        template for (constexpr auto j: idxs) {
            if (this_union.tag == j)
                this_union.destroy(j);
        }
        return *this;
    }

    constexpr trait_variant_impl& operator=(trait_variant_impl const& other) noexcept(
        ImplHolder::noexcept_copy_constructible)
        requires(ImplHolder::copy_constructible)
    {
        if (this == &other)
            return *this;
        auto&                 this_union  = extract_union_member(*this);
        auto&                 other_union = extract_union_member(other);
        static constexpr auto idxs        = make_cw_idxs<ImplHolder::count>();

        template for (constexpr auto i: idxs) {
            if (other_union.tag == i) {
                if constexpr (std::is_copy_assignable_v<typename ImplHolder::template type<i>> and
                              (not ImplHolder::noexcept_copy_constructible or
                               std::is_nothrow_copy_assignable_v<typename ImplHolder::template type<i>>)) {
                    if (this_union.tag == other_union.tag) {
                        this_union.get(i) = other_union.get(i);
                        return *this;
                    }
                }
                template for (constexpr auto j: idxs) {
                    if (this_union.tag == j)
                        this_union.destroy(j);
                }
                this_union.construct(i, other_union.get(i));
                return *this;
            }
        }
        template for (constexpr auto j: idxs) {
            if (this_union.tag == j)
                this_union.destroy(j);
        }
        return *this;
    }

    template<typename Impl>
        requires unique_alternative_of<std::remove_cvref_t<Impl>, trait_variant_impl> and
                 std::is_constructible_v<std::remove_cvref_t<Impl>, Impl&&>
    constexpr trait_variant_impl& operator=(Impl&& other) {
        using impl_t             = std::remove_cvref_t<Impl>;
        constexpr auto index     = cw<ImplHolder::template type_index<impl_t>>;
        auto&&         union_ref = extract_union_member(*this);
        auto const     tag       = union_ref.tag;
        if constexpr (std::is_assignable_v<std::remove_cvref_t<Impl>&, Impl&&>) {
            if (tag == index) {
                union_ref.get(index) = std::forward<Impl>(other);
                return *this;
            }
        }
        static constexpr auto idxs = make_cw_idxs<ImplHolder::count>();
        template for (constexpr auto i: idxs) {
            if (tag == i) {
                if (&union_ref.get(index) == &other)
                    return *this;
                union_ref.destroy(i);
            }
        }
        union_ref.construct(index, std::forward<Impl>(other));
        return *this;
    }
};
template<non_cvref ImplHolder, any_trait Trait, meta::info... MethodIdtsRefls>
auto get_var_impl_holder(trait_variant_impl<ImplHolder, Trait, MethodIdtsRefls...> const&) -> ImplHolder;


consteval auto get_var_impls(meta::info trait, meta::info impl_holder) {
    auto var_targs    = std::vector{impl_holder, trait};
    auto var_targs_c  = std::vector{impl_holder, trait};
    auto var_targs_v  = std::vector{impl_holder, trait};
    auto var_targs_cv = std::vector{impl_holder, trait};

    auto holder_targs    = std::vector<meta::info>{};
    auto holder_targs_c  = std::vector<meta::info>{};
    auto holder_targs_v  = std::vector<meta::info>{};
    auto holder_targs_cv = std::vector<meta::info>{};

    auto const all_methods = subextract_span<meta::info>(^^all_trait_methods, {trait});
    auto const groups      = subextract_span<method_reference>(^^trait_method_groups, {trait});
    for (auto const grp: groups) {
        for (auto i: stdv::iota(grp.begin_idx, grp.end_idx)) {
            auto const mem   = all_methods[i];
            auto const quals = extract_method_qualifiers(mem);

            holder_targs.push_back(mem);
            if (quals.is_const)
                holder_targs_c.push_back(mem);
            if (quals.is_volatile)
                holder_targs_v.push_back(mem);
            if (quals.is_cv())
                holder_targs_cv.push_back(mem);
        };

        if (not holder_targs.empty()) {
            var_targs.push_back(reflect_constant(meta::reflect_constant_array(holder_targs)));
            holder_targs.clear();
        }
        if (not holder_targs_c.empty()) {
            var_targs_c.push_back(reflect_constant(meta::reflect_constant_array(holder_targs_c)));
            holder_targs_c.clear();
        }
        if (not holder_targs_v.empty()) {
            var_targs_v.push_back(reflect_constant(meta::reflect_constant_array(holder_targs_v)));
            holder_targs_v.clear();
        }
        if (not holder_targs_cv.empty()) {
            var_targs_cv.push_back(reflect_constant(meta::reflect_constant_array(holder_targs_cv)));
            holder_targs_cv.clear();
        }
    }

    return std::array{
        substitute(^^trait_variant_impl, var_targs),
        substitute(^^trait_variant_impl, var_targs_c),
        substitute(^^trait_variant_impl, var_targs_v),
        substitute(^^trait_variant_impl, var_targs_cv),
    };
}

template<non_cv_trait Trait, non_ref... Impls>
inline constexpr auto var_impls = get_var_impls(^^Trait, ^^impl_holder<Impls...>);

template<typename T>
    requires requires(T v) { get_var_impl_holder(v); }
struct alternative_union_of<T> {
    static constexpr auto value = ^^decltype(get_var_impl_holder(std::declval<T>()));
};

template<any_trait Trait, implements_trait<Trait>... Impls>
using trait_variant_alias =    //
[:is_const(^^Trait) and is_volatile(^^Trait) ? var_impls<std::remove_cv_t<Trait>, Impls...>[3]
  : is_volatile(^^Trait)                     ? var_impls<std::remove_cv_t<Trait>, Impls...>[2]
  : is_const(^^Trait)                        ? var_impls<std::remove_cv_t<Trait>, Impls...>[1]
                                             : var_impls<std::remove_cv_t<Trait>, Impls...>[0]:];
template<typename T, any_trait_variant Var>
inline constexpr auto
    variant_alternative_index = [:alternative_union_of<Var>::value:] ::template type_index<T>;
}    // namespace detail::var
//
template<detail::var::any_trait_variant Var>
inline constexpr auto variant_size = [:detail::var::alternative_union_of<Var>::value:] ::count;
template<uZ I, detail::var::any_trait_variant Var>
    requires(I < variant_size<Var>)
using variant_alternative = [:[:detail::var::alternative_union_of<Var>::value:] ::type_infos[I]:];


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
    requires(I < variant_size<Var> and std::is_constructible_v<variant_alternative<I, Var>, Args...>)
constexpr auto emplace(Var& var, Args&&... args) -> decltype(auto) {
    return trait_variant_emplace<I>(var, std::forward<Args>(args)...);
}

template<typename T, detail::var::any_trait_variant Var, typename... Args>
    requires detail::var::unique_alternative_of<T, Var> and std::is_constructible_v<T, Args...>
constexpr auto emplace(Var& var, Args&&... args) -> T& {
    constexpr auto i = detail::var::variant_alternative_index<T, Var>;
    return emplace<i>(var, std::forward<Args>(args)...);
}

template<uZ I, detail::var::any_trait_variant_cvref Variant>
    requires(I < variant_size<std::remove_cvref_t<Variant>>)
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
    requires(I < variant_size<std::remove_cvref_t<Variant>>)
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
struct variant_size<Var> : std::integral_constant<std::size_t, trp::variant_size<Var>> {};

template<std::size_t I, typename Var>
    requires trp::detail::var::any_trait_variant<Var> and (I < trp::variant_size<Var>)
struct variant_alternative<I, Var> {
    using type = trp::variant_alternative<I, Var>;
};

}    // namespace std
