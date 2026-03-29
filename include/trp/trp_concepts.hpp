#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <meta>
#include <ranges>
#include <set>

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

inline constexpr auto ctx_unchecked = std::meta::access_context::unchecked();

template<typename T>
concept non_cvref = std::same_as<T, std::remove_cvref_t<T>>;

namespace detail {

template<uZ End>
consteval auto make_Is() {
    using namespace std;
    return []<uZ... Is>(index_sequence<Is...>) {
        return make_tuple(integral_constant<uZ, Is>{}...);
    }(make_index_sequence<End>{});
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
struct method_identity {
    static constexpr auto identifier       = std::string_view([:Identifier:]);
    static constexpr bool is_const         = Const;
    static constexpr bool is_volatile      = Volatile;
    static constexpr bool is_lvalue        = LVRef;
    static constexpr bool is_rvalue        = RVRef;
    static constexpr bool is_value         = Value;
    static constexpr bool is_noexcpet      = Noexcept;
    static constexpr auto return_identity  = std::type_identity<Ret>{};
    static constexpr auto param_identities = std::array{std::type_identity<Params>{}...};
    //
};
consteval auto get_method_identity(meta::info method_info) -> meta::info {
    using namespace meta;
    auto is_nsmf = is_function(method_info)                 //
                   and not is_static_member(method_info)    //
                   and not is_virtual(method_info)          //
                   and has_identifier(method_info);
    if (not is_nsmf)
        throw "get_method_identity() can only be called on non-static non-template "
              "member functions with identifiers";


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
        arguments.append_range(params | stdv::drop(1));
        return substitute(^^method_identity, arguments);
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
    arguments.append_range(params);
    return substitute(^^method_identity, arguments);
}

consteval auto nonspecial_members_of(meta::info inf) {
    return meta::members_of(inf, ctx_unchecked) | stdv::filter(std::not_fn(meta::is_special_member_function));
}

template<typename T, uZ I>
consteval bool check_constexpr_data_member() {
    constexpr auto x = [:meta::static_data_members_of(^^T, ctx_unchecked)[I]:];
    return true;
}

template<typename T>
consteval bool static_data_members_are_constexpr() {
    auto [... Is] = make_Is<meta::static_data_members_of(^^T, ctx_unchecked).size()>();
    return (check_constexpr_data_member<T, Is>() and ... and true);
}

template<typename Trait>
concept any_immediate_trait =
    std::same_as<Trait, std::remove_reference_t<Trait>>                                               //
    and stdr::empty(meta::nonstatic_data_members_of(^^Trait, ctx_unchecked))                          //
    and detail::static_data_members_are_constexpr<Trait>()                                            //
    and not stdr::empty(detail::nonspecial_members_of(^^Trait))                                       //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_virtual)                       //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_template)                      //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_operator_function)             //
    and stdr::none_of(detail::nonspecial_members_of(^^Trait), meta::is_operator_function_template)    //
    ;

template<typename... Traits>
concept any_traits =
    (... and
     (any_immediate_trait<Traits> and [:meta::substitute(^^any_traits,
                                                         meta::bases_of(^^Traits, ctx_unchecked)     //
                                                             | stdv::transform(meta::type_of)):])    //
    );
}    // namespace detail

template<typename Trait>
concept any_trait = detail::any_traits<Trait>;

template<typename Supertrait, typename Trait>
concept supertrait_of = any_trait<Supertrait> && any_trait<Trait> && false;

template<typename Supertrait, typename Trait>
concept explicit_supertrait_of = any_trait<Supertrait> && any_trait<Trait> && false;

template<typename Supertrait, typename Trait>
concept direct_supertrait_of = any_trait<Supertrait> && any_trait<Trait> && false;

namespace detail {

template<std::invocable<> F>
    requires stdr::random_access_range<std::invoke_result_t<F>>
consteval auto ce_fn_to_array(const F& f) {
    auto [... Is] = make_Is<stdr::size(f())>();
    return std::array<meta::info, sizeof...(Is)>{f()[Is]...};
}

constexpr inline struct {
    consteval static auto operator()(meta::info lhs, meta::info rhs) -> bool {
        return get_method_identity(lhs) == get_method_identity(rhs);
    }
    consteval static auto operator()(meta::info lhs) {
        return [=](meta::info rhs) { return get_method_identity(lhs) == get_method_identity(rhs); };
    }
} equal_methods;

template<typename T>
struct trait_traits {
    static constexpr auto direct_methods =
        ce_fn_to_array([] { return nonspecial_members_of(^^T) | stdr::to<std::vector<meta::info>>(); });

    static constexpr auto all_methods = [] {
        using namespace meta;
        constexpr auto get_all_methods = [] constexpr {
            auto result = std::vector<info>{};
            // parsed_methods = std::set<info>{}; // maybe use set to additinally store method spec to avoid find_if
            auto parsed_bases  = std::vector<info>{};    // replace with set when constexpr set is added
            auto append_unique = [&](auto&& methods) {
                for (auto m: methods)
                    if (stdr::find_if(result, equal_methods(m)) == result.end())
                        result.push_back(m);
            };

            // using normal loop with recursive lambda fails, use template for
            // [&](this auto self, meta::info inf) -> void {
            //     auto bases = bases_of(inf, ctx_unchecked) | stdv::transform(type_of);
            //     for (auto base: bases) {
            //         if (stdr::find(parsed_bases, base) == parsed_bases.end()) {
            //             parsed_bases.push_back(base);
            //             self(base);
            //         }
            //     }
            //     append_unique(nonspecial_members_of(inf));
            // }(^^T);

            [&]<typename U = T>(this auto self, std::type_identity<U> = {}) {
                constexpr auto bases_array =
                    ce_fn_to_array([] { return bases_of(^^U, ctx_unchecked) | stdv::transform(type_of); });
                template for (constexpr auto base: bases_array) {
                    if (stdr::find(parsed_bases, base) == parsed_bases.end()) {
                        parsed_bases.push_back(base);
                        using base_t = [:base:];
                        self(std::type_identity<base_t>{});
                    }
                }
                append_unique(nonspecial_members_of(^^U));
            }();

            return result;
        };
        return ce_fn_to_array(get_all_methods);
    }();
};


template<typename Impl, meta::info TraitMethod>
consteval auto matching_id_methods() {
    using namespace meta;
    constexpr auto get_vec = [] {
        return members_of(^^Impl, access_context::current())    //
               | stdv::filter([](auto info) {
                     return not is_special_member_function(info)    //
                            and (is_function(info)                  //
                                 or is_function_template(info))     //
                            and identifier_of(info) == identifier_of(TraitMethod);
                 })    //
               | stdr::to<std::vector<info>>();
    };
    auto ret = std::array<info, get_vec().size()>();
    stdr::copy(get_vec(), ret.begin());
    return ret;
}

template<typename Impl, meta::info ImplMethod, typename Ret, typename... Args>
constexpr bool match_method_strict() {
    if constexpr (meta::is_template(ImplMethod)) {
        return requires(Impl impl, Args... args) {
            { impl.template[:ImplMethod:](std::forward<Args>(args)...) } -> std::same_as<Ret>;
        };
    } else {
        return requires(Impl impl, Args... args) {
            { impl.[:ImplMethod:](std::forward<Args>(args)...) } -> std::same_as<Ret>;
        };
    }
};

template<typename Impl, meta::info TraitMethod>
consteval bool implements_method() {
    using namespace std;
    using namespace meta;
    constexpr auto impl_methods = matching_id_methods<Impl, TraitMethod>();
    constexpr auto get_matcher  = [](info impl_method) {
        auto match_args = vector{^^Impl, reflect_constant(impl_method), return_type_of(TraitMethod)};
        match_args.append_range(parameters_of(TraitMethod) | stdv::transform(type_of));
        return substitute(^^match_method_strict, match_args);
    };
    auto [... Is] = make_Is<impl_methods.size()>();
    return ([:get_matcher(impl_methods[Is]):]() or ...);
};

template<typename Impl, typename Trait, uZ I>
inline constexpr bool implements_methods =
    requires { requires I == trait_traits<Trait>::all_methods.size(); }    //
    or requires {
           implements_method<Impl, trait_traits<Trait>::all_methods[I]>()    //
               and implements_methods<Impl, Trait, I + 1>;
       };

}    // namespace detail
template<typename Impl, typename Trait>
concept implements_trait = any_trait<Trait> and detail::implements_methods<Impl, Trait, 0>;
}    // namespace trp
