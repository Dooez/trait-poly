#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <meta>
#include <ranges>
#include <set>
#include <type_traits>

namespace trp {
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = std::int64_t;

using u8       = uint8_t;
using u32      = uint32_t;
using u64      = std::uint64_t;
using uZ       = std::size_t;
using i64      = std::int64_t;
using iZ       = ssize_t;
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
constexpr auto cw = constant_wrapper<V>{};

static constexpr auto ctx_unchecked = std::meta::access_context::unchecked();
template<typename T>
concept cw_info = meta::has_template_arguments(^^T)                     //
                  and meta::template_of(^^T) == (^^constant_wrapper)    //
                  and
[:meta::substitute(^^std::same_as, {^^meta::info, type_of(meta::template_arguments_of (^^T)[0])}):];

template<uZ End>
consteval auto make_cw_idxs() {
    struct cw_index_sequence;
    consteval {
        constexpr auto define_cw_idx_seq = [] {
            constexpr auto cw_seq_info = ^^cw_index_sequence;
            if (meta::is_complete_type(cw_seq_info))
                return;
            constexpr auto to_mem = [](uZ i) {
                auto name = std::string("i_");
                name.resize(22);
                auto end = std::to_chars(&*name.begin() + 1, &*name.end(), i);
                if (end.ec != std::errc{})
                    throw "could not form cw_index_sequence member name";
                name.resize(end.ptr - name.data());
                return data_member_spec(meta::substitute(^^constant_wrapper, {meta::reflect_constant(i)}),
                                        {.name = name});
            };

            auto mems = std::vector<meta::info>();
            for (uZ i = 0; i < End; ++i) {    // noo iota in clang :')
                mems.push_back(to_mem(i));
            }
            meta::define_aggregate(cw_seq_info, mems);
            return;
        };
        define_cw_idx_seq();
    }
    return cw_index_sequence{};
};

// template<uZ End>
// consteval auto make_Is() {
//     const auto [... Is] = std::make_index_sequence<End>{}; // not working in clang atm
//     return std::make_tuple(std::integral_constant<uZ, Is>{}...);
// };

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
    static constexpr bool is_lvalue        = LVRef;
    static constexpr bool is_rvalue        = RVRef;
    static constexpr bool is_value         = Value;
    static constexpr bool is_noexcept      = Noexcept;
    using return_type                      = Ret;
    static constexpr auto param_infos      = std::array<meta::info, sizeof...(Params)>{^^Params...};
    static constexpr auto param_identities = std::make_tuple(std::type_identity<Params>{}...);

    static consteval auto add_obj_cv(meta::info inf) {
        if (is_volatile)
            inf = meta::add_volatile(inf);
        if (is_const)
            inf = meta::add_const(inf);
        return inf;
    };
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

template<auto F>
inline constexpr auto ce_fn_to_array = [] {
    using T = stdr::range_value_t<std::invoke_result_t<decltype(F)>>;
    struct return_tup;
    consteval {
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
        meta::define_aggregate(^^return_tup, F() | stdv::transform(info_to_member));
    }
    auto [... values] = return_tup{};
    return std::array<T, sizeof...(values)>{values...};
}();

template<typename T>
consteval bool static_data_members_are_constexpr() {
    constexpr auto mems = ce_fn_to_array<[] { return meta::static_data_members_of(^^T, ctx_unchecked); }>;
    auto [... Is]       = make_cw_idxs<mems.size()>();
    return (check_constexpr_static_data_member<T, mems[Is]>() and ... and true);
}

template<typename Trait>
concept any_immediate_trait =
    std::same_as<Trait, std::remove_reference_t<Trait>>                         //
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
    ;

template<meta::info Self, typename... Traits>
concept any_traits =
    (... and
     (any_immediate_trait<Traits> and [:meta::substitute(Self,
                                                         [] {
                                                             auto res =
                                                                 std::vector{meta::reflect_constant(Self)};
                                                             res.append_range(
                                                                 meta::bases_of(^^Traits, ctx_unchecked)    //
                                                                 | stdv::transform(meta::type_of));
                                                             return res;
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
concept any_trait = detail::any_traits<^^detail::any_traits, Trait>;


namespace detail {


template<typename T>
struct trait_traits {
    static constexpr auto direct_base_types = ce_fn_to_array<[] {
        return meta::bases_of(^^T, ctx_unchecked)    //
               | stdv::transform(meta::type_of)      //
               | stdr::to<std::vector<meta::info>>();
    }>;

    static constexpr auto direct_methods = [] {
        using namespace meta;
        constexpr auto get_direct_methods = [] {
            constexpr auto is_valid_method = [](auto method) static {
                return is_function(method)                                 //
                       and not meta::is_special_member_function(method)    //
                       and (is_const(method) or not is_const(^^T))         //
                       and (is_volatile(method) or not is_volatile(^^T));
            };
            return meta::members_of(^^T, ctx_unchecked)    //
                   | stdv::filter(is_valid_method)         //
                   | stdv::transform(method_identity)      //
                   | stdr::to<std::vector<info>>();
        };
        return ce_fn_to_array<get_direct_methods>;
    }();

    static constexpr auto all_methods = [] {
        using namespace meta;
        constexpr auto get_all_methods = [] {
            auto result        = direct_methods | stdr::to<std::vector<info>>();
            auto append_unique = [&](auto&& method_idts) {
                for (auto m: method_idts)
                    if (not stdr::contains(result, m))
                        result.push_back(m);
            };
            template for (constexpr auto base: direct_base_types) {
                using base_t = [:copy_cv_to(^^T, base):];
                append_unique(trait_traits<base_t>::all_methods);
            }
            return result;
        };
        return ce_fn_to_array<get_all_methods>;
    }();
};
}    // namespace detail

template<typename Supertrait, typename Trait>
concept supertrait_of = any_trait<Supertrait>    //
                        and any_trait<Trait>     //
                        and ([] {
                                for (auto m: detail::trait_traits<Supertrait>::all_methods) {
                                    if (not stdr::contains(detail::trait_traits<Trait>::all_methods, m))
                                        return false;
                                }
                                return true;
                            }());
template<typename Supertrait, typename Trait>
concept direct_supertrait_of =
    supertrait_of<Supertrait, Trait> and
    stdr::contains(detail::trait_traits<Trait>::direct_base_types, meta::remove_cv(^^Supertrait));
template<typename Supertrait, typename Trait>
concept explicit_supertrait_of = supertrait_of<Supertrait, Trait> and std::derived_from<Trait, Supertrait>;


namespace detail {
template<non_ref Impl, auto Id>
constexpr auto matching_id_public_members = [] {
    using namespace meta;
    constexpr auto get_vec = [] {
        auto result = std::vector<info>{};
        [&](this auto self, cw_info auto type) {
            auto cur_members =
                members_of(type, access_context::unprivileged())    //
                | stdv::filter(std::not_fn(is_static_member))       //
                | stdv::filter([](auto info) {
                      return has_identifier(info) and identifier_of(info) == Id;
                  });
            for (auto info: cur_members) {
                if (not stdr::contains(result, info))
                    result.push_back(info);
            }
            constexpr auto has_bases = not stdr::empty(bases_of(type, access_context::unprivileged()));
            if constexpr (has_bases) {
                // separate overload resolution check should be performed, to ensure that bases on the same level do not contain identical methods

                static constexpr auto bases = ce_fn_to_array<[=] {
                    return bases_of(type, access_context::unprivileged()) | stdv::transform(type_of);
                }>;
                template for (constexpr auto base: bases) {
                    self(cw<base>);
                }
            }
        }(cw<^^Impl>);
        return result;
    };
    return ce_fn_to_array<get_vec>;
}();

template<meta::info ImplMethod, any_method_idt MethodId>
constexpr bool match_method_strict() {
    using return_type       = MethodId::return_type;
    using impl_invocation_t = [:MethodId::add_obj_cv(meta::parent_of(ImplMethod)):];
    auto [... arg_ids]      = MethodId::param_identities;

    if constexpr (meta::is_template(ImplMethod)) {
        return requires(impl_invocation_t impl, decltype(arg_ids)::type... args) {
            {
                impl.template[:ImplMethod:](std::forward<decltype(arg_ids)::type>(args)...)
            } -> std::same_as<return_type>;
        };
    } else {
        return requires(impl_invocation_t impl, decltype(arg_ids)::type... args) {
            {
                impl.[:ImplMethod:](std::forward<decltype(arg_ids)::type>(args)...)
            } -> std::same_as<return_type>;
        };
    }
};

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

template<typename Impl, typename MethodIdt>
concept implements_method =
    non_ref<Impl>                    //
    and any_method_idt<MethodIdt>    //
    and
    ([] {
        using namespace std;
        using namespace meta;
        constexpr auto matches = [=](cw_info auto impl_method) {
            return [:substitute(^^strictly_matches, {^^Impl, reflect_constant(impl_method), ^^MethodIdt}):];
        };
        static constexpr auto members = matching_id_public_members<Impl, MethodIdt::identifier>;
        template for (constexpr auto m: members) {
            if (matches(cw<m>))
                return true;
        }
        return false;
    }());


template<meta::info Self, typename Impl, typename Trait, uZ I = 0>
concept implements_methods =
    requires { requires I == trait_traits<Trait>::all_methods.size(); }                             //
    or ([:meta::substitute(^^implements_method, {^^Impl, trait_traits<Trait>::all_methods[I]}):]    //
        and
           [:meta::substitute(
                 Self, {meta::reflect_constant(Self), ^^Impl, ^^Trait, meta::reflect_constant(I + 1)}):]);

}    // namespace detail

template<typename Impl, typename Trait>
concept implements_trait =
    any_trait<Trait> and detail::implements_methods<^^detail::implements_methods, Impl, Trait>;

}    // namespace trp
