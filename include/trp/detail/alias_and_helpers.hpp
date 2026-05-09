#pragma once
#include <algorithm>
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

static constexpr auto ctx_unchecked = meta::access_context::unchecked();

template<typename T>
concept cw_info =
    has_template_arguments(^^T)                     //
    and template_of(^^T) == (^^constant_wrapper)    //
    and extract<bool>(substitute(^^std::same_as, {^^meta::info, type_of(template_arguments_of (^^T)[0])}));

consteval auto anon_member_spec(meta::info r) -> meta::info {
#if defined(__clang__)
    return data_member_spec(r, {});
#else
    return data_member_spec(r, {.name = "_"});
#endif
}

template<non_cvref... Ts>
struct aggregate_definer {
    struct aggregate;
    consteval {
        define_aggregate(^^aggregate,
                         std::array<meta::info, sizeof...(Ts)>{^^Ts...} | stdv::transform(anon_member_spec));
    }
};
template<non_cvref... Ts>
using anon_aggregate = aggregate_definer<Ts...>::aggregate;

template<typename... Ts>
consteval auto make_aggregate(Ts&&... vs) {
    using aggregate = aggregate_definer<std::remove_cvref_t<Ts>...>::aggregate;
    return aggregate{std::forward<Ts>(vs)...};
};

template<uZ End>
consteval auto make_cw_idxs() {
    constexpr auto value_to_cw_member = [](auto v) {
        return substitute(^^constant_wrapper, {meta::reflect_constant(v)});
    };
    using cw_index_sequence = [:substitute(^^anon_aggregate,
                                           stdv::iota(0UZ, End) | stdv::transform(value_to_cw_member)):];
    return cw_index_sequence{};
};
template<meta::info RefInfo>
inline constexpr auto extract_size = stdr::size([:RefInfo:]);
template<meta::info RefInfo>
inline constexpr auto extract_ptr = stdr::data([:RefInfo:]);

template<meta::reflection_range R = std::initializer_list<meta::info>>
consteval auto subextract_info_span(meta::info r, R const& targs) -> std::span<meta::info const> {
    auto const src_span = meta::reflect_constant(meta::substitute(r, targs));
    return std::span<meta::info const>{extract<meta::info const*>(substitute(^^extract_ptr, {src_span})),
                                       extract<uZ>(substitute(^^extract_size, {src_span}))};
}

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
            inf = add_volatile(inf);
        if (is_const)
            inf = add_const(inf);
        return inf;
    };

    using wrapper_fptr_type = auto (*)(void*, Params...) noexcept(Noexcept) -> Ret;
};
consteval auto method_identity(meta::info method_info) -> meta::info {
    auto is_nsmf = is_function(method_info)                 //
                   and not is_static_member(method_info)    //
                   and not is_virtual(method_info)          //
                   and has_identifier(method_info);
    if (not is_nsmf) {
        throw "get_method_identity() can only be called on non-static non-template "
              "member functions with identifiers";
    }


    auto const identifier   = meta::reflect_constant_string(identifier_of(method_info));
    auto const is_noexcept_ = meta::reflect_constant(is_noexcept(method_info));
    auto const ret          = return_type_of(method_info);
    auto       params       = parameters_of(method_info);
    if (not params.empty() and is_explicit_object_parameter(params[0])) {
        auto const eop          = params[0];
        auto const eop_t        = type_of(eop);
        auto const is_const_    = meta::reflect_constant(is_const(remove_reference(eop_t)));
        auto const is_volatile_ = meta::reflect_constant(is_volatile(remove_reference(eop_t)));
        auto const is_lvref     = meta::reflect_constant(is_lvalue_reference_type(eop_t));
        auto const is_rvref     = meta::reflect_constant(is_rvalue_reference_type(eop_t));
        auto const is_value_    = meta::reflect_constant(not is_lvalue_reference_type(eop_t)    //
                                                      and not is_rvalue_reference_type(eop_t));
        auto       arguments    = std::vector{identifier,    //
                                     is_const_,
                                     is_volatile_,
                                     is_lvref,
                                     is_rvref,
                                     is_value_,
                                     is_noexcept_,
                                     ret};
        arguments.append_range(params | stdv::drop(1) | stdv::transform(meta::type_of));
        return substitute(^^method_identity_t, arguments);
    }
    auto const is_const_    = meta::reflect_constant(is_const(method_info));
    auto const is_volatile_ = meta::reflect_constant(is_volatile(method_info));
    auto const is_lvref     = meta::reflect_constant(is_lvalue_reference_qualified(method_info));
    auto const is_rvref     = meta::reflect_constant(is_rvalue_reference_qualified(method_info));
    auto const is_value_    = meta::reflect_constant(false);
    auto       arguments    = std::vector{identifier,    //
                                 is_const_,
                                 is_volatile_,
                                 is_lvref,
                                 is_rvref,
                                 is_value_,
                                 is_noexcept_,
                                 ret};
    arguments.append_range(params | stdv::transform(meta::type_of));
    return substitute(^^method_identity_t, arguments);
}

template<typename T>
concept any_method_idt = has_template_arguments(^^T) and template_of(^^T) == ^^method_identity_t;

template<typename T>
concept trait_method_idt = any_method_idt<T> and not(T::is_rvalue) and not(T::is_value);

namespace concepts {
template<typename MethodIdt>
concept is_const_idt = any_method_idt<MethodIdt> && MethodIdt::is_const;
template<typename MethodIdt>
concept is_volatile_idt = any_method_idt<MethodIdt> && MethodIdt::is_volatile;
template<typename MethodIdt>
concept is_cv_idt = any_method_idt<MethodIdt> && MethodIdt::is_cv;
}    // namespace concepts

consteval bool is_const_idt(meta::info idt) {
    return extract<bool>(substitute(^^concepts::is_const_idt, {idt}));
}
consteval bool is_volatile_idt(meta::info idt) {
    return extract<bool>(substitute(^^concepts::is_volatile_idt, {idt}));
}
consteval bool is_cv_idt(meta::info idt) {
    return extract<bool>(substitute(^^concepts::is_cv_idt, {idt}));
}

consteval auto copy_cv_to(meta::info proto, meta::info type) {
    if (is_const(proto))
        type = add_const(type);
    if (is_volatile(proto))
        type = add_volatile(type);
    return type;
}

template<non_cvref T>
inline constexpr auto nonspecial_members = std::define_static_array(
    members_of(^^T, ctx_unchecked) | stdv::filter(std::not_fn(meta::is_special_member_function)));

template<non_cvref T>
inline constexpr auto direct_base_types = std::define_static_array(
    bases_of(^^T, meta::access_context::unprivileged()) | stdv::transform(meta::type_of));

template<typename T>
inline constexpr auto direct_trait_methods = [] {
    constexpr auto is_valid_method = [](meta::info method) {
        return is_function(method)                            //
               and not is_special_member_function(method)     //
               and (is_const(method) or not is_const(^^T))    //
               and (is_volatile(method) or not is_volatile(^^T));
    };
    return define_static_array(members_of(^^T, std::meta::access_context::unprivileged())    //
                               | stdv::filter(is_valid_method)                               //
                               | stdv::transform(method_identity));
}();
template<typename T>
inline constexpr auto all_trait_methods = [] {
    auto       result        = direct_trait_methods<T> | stdr::to<std::vector<meta::info>>();
    auto const append_unique = [&](auto&& method_idts) {
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

template<non_ref T>
struct unique_id_struct {
    inline static char value{};
};
}    // namespace detail
}    // namespace trp
