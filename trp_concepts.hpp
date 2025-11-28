#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <meta>
#include <ranges>

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

inline constexpr auto ctx_unchecked = std::meta::access_context::unchecked();

template<typename Trait>
concept any_trait =
    stdr::empty(std::meta::nonstatic_data_members_of(^^Trait, ctx_unchecked))     //
    and stdr::empty(std::meta::static_data_members_of(^^Trait, ctx_unchecked))    //
    and not stdr::empty(std::meta::members_of(^^Trait, ctx_unchecked) |
                        stdv::filter(std::not_fn(std::meta::is_special_member_function)))    //
    and stdr::none_of(std::meta::members_of(^^Trait, ctx_unchecked) |
                          stdv::filter(std::not_fn(std::meta::is_special_member_function)),
                      std::meta::is_virtual)    //
    and stdr::none_of(std::meta::members_of(^^Trait, ctx_unchecked) |
                          stdv::filter(std::not_fn(std::meta::is_special_member_function)),
                      std::meta::is_template)    //
    ;

template<typename T>
struct trait_traits {
    static constexpr auto methods = [] {
        using namespace std;
        using namespace std::meta;
        constexpr auto n = [] {
            auto trait_members = members_of(^^T, ctx_unchecked)                        //
                                 | stdv::filter(not_fn(is_special_member_function))    //
                                 | stdr::to<vector<info>>();
            return trait_members.size();
        }();
        auto methods = array<info, n>{};
        stdr::copy(members_of(^^T, ctx_unchecked)    //
                       | stdv::filter(not_fn(is_special_member_function)),
                   methods.begin());
        stdr::sort(methods, {}, identifier_of);
        return methods;
    }();
};


namespace detail {
template<typename T, std::meta::info TraitMethod>
consteval auto get_callable_members() {
    using namespace std;
    using namespace std::meta;
    constexpr auto get_vec = [] {
        return members_of(^^T, access_context::current()) | stdv::filter([](auto info) {
                   return not is_special_member_function(info)                          //
                          && (meta::is_function(info) or is_function_template(info))    //
                          && identifier_of(info) == identifier_of(TraitMethod);
               }) |
               stdr::to<vector<info>>();
    };
    constexpr auto n = get_vec().size();

    auto mems = get_vec();
    auto ret  = array<info, n>();
    stdr::copy(mems, ret.begin());
    return ret;
}

template<typename Impl, std::meta::info ImplMethod, typename Ret, typename... Args>
constexpr bool match_method() {
    if constexpr (std::meta::is_template(ImplMethod)) {
        return requires(Impl impl, Args&&... args) {
            { impl.template[:ImplMethod:](std::forward<Args>(args)...) } -> std::same_as<Ret>;
        };
    } else {
        return requires(Impl impl, Args&&... args) {
            { impl.[:ImplMethod:](std::forward<Args>(args)...) } -> std::same_as<Ret>;
        };
    }
};
template<typename Impl, std::meta::info TraitMethod, typename Ret, typename... Args>
constexpr bool has_method_() {
    using namespace std;
    constexpr auto impl_mems = get_callable_members<Impl, TraitMethod>();
    auto [... Is]            = []<uZ... Is>(index_sequence<Is...>) {
        return make_tuple(integral_constant<uZ, Is>{}...);    // TODO: modernize new integral constant iface
    }(make_index_sequence<impl_mems.size()>{});
    return (match_method<Impl, impl_mems[Is], Ret, Args...>() || ...);
}

template<typename Impl, std::meta::info TraitMethod>
consteval bool has_method() {
    using namespace std;
    using namespace std::meta;
    constexpr auto has_method_impl = [] {
        auto [... args] = [] {
            constexpr auto n         = (parameters_of(TraitMethod) | stdv::transform(type_of)).size();
            auto           arg_array = std::array<info, n>{};
            stdr::copy(parameters_of(TraitMethod) | stdv::transform(type_of), arg_array.begin());
            return arg_array;
        }();
        return substitute(^^has_method_,
                          {^^Impl,
                           reflect_constant(TraitMethod),
                           return_type_of(TraitMethod),
                           args...});    // couldn't make splicing work
    }();
    return [:has_method_impl:]();
};


template<typename Impl, any_trait Trait, uZ I>
inline constexpr bool implements_methods =
    requires { requires I == trait_traits<Trait>::methods.size(); }    //
    || requires {
           requires(I < trait_traits<Trait>::methods.size()) &&
                       has_method<Impl, trait_traits<Trait>::methods[I]>() &&
                       implements_methods<Impl, Trait, I + 1>;
       };

}    // namespace detail
template<typename Impl, typename Trait>
concept implements_trait = any_trait<Trait> && detail::implements_methods<Impl, Trait, 0>;
}    // namespace trp
