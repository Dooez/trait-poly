#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <meta>
#include <ranges>
#include <type_traits>

namespace trp {
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = std::int64_t;
using iZ  = std::ptrdiff_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = std::uint64_t;
using uZ  = std::size_t;

using f32      = float;
using f64      = double;
namespace stdr = std::ranges;
namespace stdv = std::views;
namespace meta = std::meta;

template<auto V>
struct constant_wrapper {
    using type       = constant_wrapper;
    using value_type = decltype(V);

    static constexpr auto value = V;

    constexpr operator decltype(auto)() const noexcept {
        return value;
    }
};

template<typename T>
concept non_cvref = std::same_as<T, std::remove_cvref_t<T>>;
template<typename T>
concept non_ref = std::same_as<T, std::remove_reference_t<T>>;

namespace detail {

template<auto V>
inline constexpr auto cw = constant_wrapper<V>{};

static constexpr auto ctx_unchecked = std::meta::access_context::unchecked();
template<typename T>
concept cw_info = meta::has_template_arguments(^^T)                     //
                  and meta::template_of(^^T) == (^^constant_wrapper)    //
                  and
[:meta::substitute(^^std::same_as, {^^meta::info, type_of(meta::template_arguments_of (^^T)[0])}):];

template<typename... Ts>
consteval auto make_aggregate(Ts&&... vs) {
    struct aggregate;
    consteval {
        constexpr auto agg_info = ^^aggregate;
        if (meta::is_complete_type(agg_info))
            return;
        auto cnt            = 0UZ;
        auto id_storage     = std::array<char, 21>{"m"};
        auto info_to_member = [&](auto info) mutable {
            auto id_end = std::to_chars(&*(id_storage.begin() + 1), &*id_storage.end(), cnt++);
            if (id_end.ec != std::errc{})
                throw "Error while forming member name";
            auto id = std::string_view(id_storage.data(), id_end.ptr);
            return meta::data_member_spec(info, {.name = id});
        };
        meta::define_aggregate(
            agg_info, std::array<meta::info, sizeof...(Ts)>{^^Ts...} | stdv::transform(info_to_member));
    }
    return aggregate{std::forward<Ts>(vs)...};
};

template<uZ End>
consteval auto make_cw_idxs() {
    struct cw_index_sequence;
    consteval {
        constexpr auto cw_seq_info = ^^cw_index_sequence;
        if (meta::is_complete_type(cw_seq_info))
            return;
        auto cnt            = 0UZ;
        auto id_storage     = std::array<char, 21>{"m"};
        auto info_to_member = [&](auto info) mutable {
            auto id_end = std::to_chars(&*(id_storage.begin() + 1), &*id_storage.end(), cnt++);
            if (id_end.ec != std::errc{})
                throw "Error while forming member name";
            auto id = std::string_view(id_storage.data(), id_end.ptr);
            return meta::data_member_spec(
                meta::substitute(^^constant_wrapper, {meta::reflect_constant(info)}), {.name = id});
        };
        meta::define_aggregate(cw_seq_info, stdv::iota(0UZ, End) | stdv::transform(info_to_member));
    }
    return cw_index_sequence{};
};

template<auto Identifier,
         bool Const,
         bool Volatile,
         bool LVRef,
         bool RVRef,
         bool Value,
         bool Noexcept,
         typename Ret,
         typename... Params>
struct method_identity_t {
    static constexpr auto identifier       = Identifier;
    static constexpr bool is_const         = Const;
    static constexpr bool is_volatile      = Volatile;
    static constexpr bool is_cv            = Const and Volatile;
    static constexpr bool is_lvalue        = LVRef;
    static constexpr bool is_rvalue        = RVRef;
    static constexpr bool is_value         = Value;
    static constexpr bool is_noexcept      = Noexcept;
    using return_type                      = Ret;
    static constexpr auto param_infos      = std::array<meta::info, sizeof...(Params)>{^^Params...};
    static constexpr auto param_identities = make_aggregate(std::type_identity<Params>{}...);

    static consteval auto add_obj_cv(meta::info inf) {
        if (is_volatile)
            inf = meta::add_volatile(inf);
        if (is_const)
            inf = meta::add_const(inf);
        return inf;
    };

    using wrapper_fptr_type = auto (*)(void*, Params...) noexcept(Noexcept) -> Ret;
};
consteval auto method_identity(meta::info method_info) -> meta::info {
    using namespace meta;
    auto is_nsmf = is_function(method_info)                 //
                   and not is_static_member(method_info)    //
                   and not is_virtual(method_info)          //
                   and has_identifier(method_info);
    if (not is_nsmf) {
        throw "get_method_identity() can only be called on non-static non-template "
              "member functions with identifiers";
    }


    const auto identifier   = reflect_constant_string(identifier_of(method_info));
    const auto is_noexcept_ = reflect_constant(is_noexcept(method_info));
    const auto ret          = return_type_of(method_info);
    auto       params       = parameters_of(method_info);
    if (not params.empty() and is_explicit_object_parameter(params[0])) {
        const auto eop          = params[0];
        const auto eop_t        = type_of(eop);
        const auto is_const_    = reflect_constant(is_const(remove_reference(eop_t)));
        const auto is_volatile_ = reflect_constant(is_volatile(eop_t));
        const auto is_lvref     = reflect_constant(is_lvalue_reference_type(eop_t));
        const auto is_rvref     = reflect_constant(is_rvalue_reference_type(eop_t));
        const auto is_value_    = reflect_constant(not is_lvalue_reference_type(eop_t)    //
                                                and not is_rvalue_reference_type(eop_t));
        auto       arguments    = std::vector{identifier,    //
                                     is_const_,
                                     is_volatile_,
                                     is_lvref,
                                     is_rvref,
                                     is_value_,
                                     is_noexcept_,
                                     ret};
        arguments.append_range(params | stdv::drop(1) | stdv::transform(type_of));
        return substitute(^^method_identity_t, arguments);
    }
    const auto is_const_    = reflect_constant(is_const(method_info));
    const auto is_volatile_ = reflect_constant(is_volatile(method_info));
    const auto is_lvref     = reflect_constant(is_lvalue_reference_qualified(method_info));
    const auto is_rvref     = reflect_constant(is_rvalue_reference_qualified(method_info));
    const auto is_value_    = reflect_constant(false);
    auto       arguments    = std::vector{identifier,    //
                                 is_const_,
                                 is_volatile_,
                                 is_lvref,
                                 is_rvref,
                                 is_value_,
                                 is_noexcept_,
                                 ret};
    arguments.append_range(params | stdv::transform(type_of));
    return substitute(^^method_identity_t, arguments);
}
template<typename T>
concept any_method_idt = meta::has_template_arguments(^^T) and meta::template_of(^^T) == ^^method_identity_t;


template<typename T>
inline constexpr auto nonspecial_members = std::define_static_array(
    meta::members_of(^^T, ctx_unchecked) | stdv::filter(std::not_fn(meta::is_special_member_function)));

consteval auto nonspecial_members_of(meta::info inf) {
    return meta::members_of(inf, ctx_unchecked) | stdv::filter(std::not_fn(meta::is_special_member_function));
}

template<typename T, meta::info M>
consteval bool check_constexpr_static_data_member() {
#ifdef TRP_CHECK_CESDM
    return requires { cw<([:M:])>; };
#else
    return false;
#endif
}

template<template<typename> typename DefaultImpl>
struct default_impl_annotation_t {};

consteval bool is_default_impl_annotation(meta::info r) {
    return meta::is_annotation(r) and meta::template_of(meta::type_of(r)) == ^^default_impl_annotation_t;
}

template<non_cvref T>
inline constexpr auto direct_base_types = std::define_static_array(
    meta::bases_of(^^T, meta::access_context::unprivileged()) | stdv::transform(meta::type_of));

template<typename T>
consteval bool static_data_members_are_constexpr() {
    constexpr auto mems = std::define_static_array(meta::static_data_members_of(^^T, ctx_unchecked));
    auto [... Is]       = make_cw_idxs<mems.size()>();
    return (check_constexpr_static_data_member<T, mems[Is]>() and ... and true);
}

template<typename Trait>
concept any_immediate_trait =
    non_ref<Trait>                                                              //
    and stdr::empty(meta::nonstatic_data_members_of(^^Trait, ctx_unchecked))    //
    and detail::static_data_members_are_constexpr<Trait>()                      // only in gcc atm
    and stdr::none_of(meta::members_of(^^Trait, ctx_unchecked),
                      [](auto m) {
                          return meta::is_special_member_function(m)    //
                                 and not meta::is_defaulted(m);
                      })                                                                              //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_virtual)                       //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_template)                      //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_operator_function)             //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_operator_function_template)    //
    and (stdr::size(direct_base_types<std::remove_cv_t<Trait>>) ==
         stdr::size(meta::bases_of(^^Trait, meta::access_context::unchecked())))         //
    and (stdr::count_if(meta::annotations_of(^^Trait), is_default_impl_annotation) <= 1)    //
    ;

template<meta::info Self, typename... Traits>
concept any_traits =
    (... and
     (any_immediate_trait<Traits> and [:meta::substitute(Self,
                                                         [] {
                                                             auto args =
                                                                 std::vector{meta::reflect_constant(Self)};
                                                             args.append_range(
                                                                 direct_base_types<std::remove_cv_t<Traits>>);
                                                             return args;
                                                         }()):])    //
    );

consteval auto copy_cv_to(meta::info proto, meta::info type) {
    if (meta::is_const(proto))
        type = meta::add_const(type);
    if (meta::is_volatile(proto))
        type = meta::add_volatile(type);
    return type;
}

}    // namespace detail

template<typename Trait>
concept any_trait = std::is_class_v<Trait> and detail::any_traits<^^detail::any_traits, Trait>;

template<typename Trait>
concept non_cv_trait = any_trait<Trait> and non_cvref<Trait>;

namespace detail {
template<typename T>
inline constexpr auto all_trait_methods = [] {
    using namespace meta;
    constexpr auto is_valid_method = [](auto method) static {
        return is_function(method)                                 //
               and not meta::is_special_member_function(method)    //
               and (is_const(method) or not is_const(^^T))         //
               and (is_volatile(method) or not is_volatile(^^T));
    };
    auto result = meta::members_of(^^T, ctx_unchecked)    //
                  | stdv::filter(is_valid_method)         //
                  | stdv::transform(method_identity)      //
                  | stdr::to<std::vector<info>>();
    auto append_unique = [&](auto&& method_idts) {
        for (auto m: method_idts)
            if (not stdr::contains(result, m))
                result.push_back(m);
    };
    template for (constexpr auto base: direct_base_types<std::remove_cv_t<T>>) {
        using base_t = [:copy_cv_to(^^T, base):];
        append_unique(all_trait_methods<base_t>);
    }
    return define_static_array(result);
}();

}    // namespace detail

template<typename Supertrait, typename Trait>
concept supertrait_of = any_trait<Supertrait>    //
                        and any_trait<Trait>     //
                        and ([] {
                                for (auto m: detail::all_trait_methods<Supertrait>) {
                                    if (not stdr::contains(detail::all_trait_methods<Trait>, m))
                                        return false;
                                }
                                return true;
                            }());
template<typename Supertrait, typename Trait>
concept direct_supertrait_of =
    supertrait_of<Supertrait, Trait> and
    stdr::contains(detail::direct_base_types<std::remove_cv_t<Trait>>, meta::remove_cv(^^Supertrait));
template<typename Supertrait, typename Trait>
concept explicit_supertrait_of = supertrait_of<Supertrait, Trait> and std::derived_from<Trait, Supertrait>;


namespace detail {
template<non_cvref Impl, auto Id>
inline constexpr auto matching_id_public_members = [] {
    using namespace meta;
    auto result = meta::members_of(^^Impl, meta::access_context::unprivileged())    //
                  | stdv::filter([](auto info) {
                        return meta::has_identifier(info) and meta::identifier_of(info) == Id;
                    })    //
                  | stdr::to<std::vector<meta::info>>();
    template for (constexpr auto base: direct_base_types<Impl>) {
        using base_t = [:base:];
        for (auto m: matching_id_public_members<base_t, Id>)
            if (not stdr::contains(result, m))
                result.push_back(m);
    }
    return std::define_static_array(result);
}();

template<non_ref Impl, meta::info ImplMethod, any_method_idt MethodId>
inline constexpr bool strictly_matches = [] {
    using return_type       = MethodId::return_type;
    using impl_invocation_t = [:MethodId::add_obj_cv(^^Impl):];
    auto [... arg_ids]      = MethodId::param_identities;
    if constexpr (meta::is_template(ImplMethod)) {
        return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
            {
                impl.template[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
            } -> std::same_as<return_type>;
        };
    } else {
        return requires(impl_invocation_t impl, typename decltype(arg_ids)::type... args) {
            {
                impl.[:ImplMethod:](std::forward<typename decltype(arg_ids)::type>(args)...)
            } -> std::same_as<return_type>;
        };
    }
}();

template<non_ref Impl, any_method_idt TraitMethod>
inline constexpr auto matching_impl_method = [] {
    for (auto m: matching_id_public_members<std::remove_cv_t<Impl>, TraitMethod::identifier>) {
        auto matches =
            meta::substitute(^^strictly_matches, {^^Impl, meta::reflect_constant(m), ^^TraitMethod});
        if (meta::extract<bool>(matches))
            return m;
    }
    return meta::info{};
}();

template<typename Impl, typename MethodIdt>
concept implements_method = non_ref<Impl>                    //
                            and any_method_idt<MethodIdt>    //
                            and (matching_impl_method<Impl, MethodIdt> != meta::info{});

template<non_ref T>
struct unique_id_struct {
    static inline char value{};
};
}    // namespace detail

}    // namespace trp
