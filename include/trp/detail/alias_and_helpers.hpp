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
template<meta::info... DMSpecs>
inline constexpr meta::info define_new_aggregate_impl = [] {
    struct aggregate;
    meta::define_aggregate(^^aggregate, {DMSpecs...});
    return ^^aggregate;
}();

template<std::meta::reflection_range R = std::initializer_list<meta::info>>
consteval auto define_new_aggregate(R&& r) -> meta::info {
    return meta::extract<meta::info>(
        meta::substitute(^^define_new_aggregate_impl, std::forward<R>(r)));
}


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
concept trait_method_idt = any_method_idt<T> and not(T::is_lvalue) and not(T::is_rvalue) and not(T::is_value);

namespace concepts {
template<typename MethodIdt>
concept is_const_idt = any_method_idt<MethodIdt> && MethodIdt::is_const;
template<typename MethodIdt>
concept is_volatile_idt = any_method_idt<MethodIdt> && MethodIdt::is_volatile;
template<typename MethodIdt>
concept is_cv_idt = any_method_idt<MethodIdt> && MethodIdt::is_cv;
}    // namespace concepts

consteval bool is_const_idt(meta::info idt) {
    return meta::extract<bool>(meta::substitute(^^concepts::is_const_idt, {idt}));
}
consteval bool is_volatile_idt(meta::info idt) {
    return meta::extract<bool>(meta::substitute(^^concepts::is_volatile_idt, {idt}));
}
consteval bool is_cv_idt(meta::info idt) {
    return meta::extract<bool>(meta::substitute(^^concepts::is_cv_idt, {idt}));
}

consteval auto copy_cv_to(meta::info proto, meta::info type) {
    if (meta::is_const(proto))
        type = meta::add_const(type);
    if (meta::is_volatile(proto))
        type = meta::add_volatile(type);
    return type;
}

template<non_cvref T>
inline constexpr auto nonspecial_members = std::define_static_array(
    meta::members_of(^^T, ctx_unchecked) | stdv::filter(std::not_fn(meta::is_special_member_function)));

template<non_cvref T>
inline constexpr auto direct_base_types = std::define_static_array(
    meta::bases_of(^^T, meta::access_context::unprivileged()) | stdv::transform(meta::type_of));

template<typename T>
inline constexpr auto direct_trait_methods = [] {
    using namespace meta;
    constexpr auto is_valid_method = [](auto method) static {
        return is_function(method)                                 //
               and not meta::is_special_member_function(method)    //
               and (is_const(method) or not is_const(^^T))         //
               and (is_volatile(method) or not is_volatile(^^T));
    };
    return define_static_array(members_of(^^T, std::meta::access_context::unprivileged())    //
                               | stdv::filter(is_valid_method)                               //
                               | stdv::transform(method_identity));
}();
template<typename T>
inline constexpr auto all_trait_methods = [] {
    using namespace meta;
    constexpr auto is_valid_method = [](auto method) static {
        return is_function(method)                                 //
               and not meta::is_special_member_function(method)    //
               and (is_const(method) or not is_const(^^T))         //
               and (is_volatile(method) or not is_volatile(^^T));
    };
    auto result        = direct_trait_methods<T> | stdr::to<std::vector<info>>();
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

template<non_ref T>
struct unique_id_struct {
    static inline char value{};
};
}    // namespace detail
}    // namespace trp
