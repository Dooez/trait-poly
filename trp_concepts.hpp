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
namespace detail {

template<uZ End>
consteval auto make_Is() {
    using namespace std;
    return []<uZ... Is>(index_sequence<Is...>) {
        return make_tuple(integral_constant<uZ, Is>{}...);    // TODO: modernize new integral constant iface
    }(make_index_sequence<End>{});
};

template<typename T>
struct trait_traits {
    static constexpr auto methods = [] {
        using namespace std;
        using namespace std::meta;
        constexpr auto get_nonspecial = [] {
            return members_of(^^T, ctx_unchecked)                        //
                   | stdv::filter(not_fn(is_special_member_function))    //
                   | stdr::to<vector<info>>();
        };
        auto methods = array<info, get_nonspecial().size()>{};
        stdr::copy(get_nonspecial(), methods.begin());
        stdr::sort(methods, {}, identifier_of);
        return methods;
    }();
};


template<typename Impl, std::meta::info TraitMethod>
consteval auto matching_id_methods() {
    using namespace std;
    using namespace std::meta;
    constexpr auto get_vec = [] {
        return members_of(^^Impl, access_context::current()) | stdv::filter([](auto info) {
                   return not is_special_member_function(info)                          //
                          && (meta::is_function(info) or is_function_template(info))    //
                          && identifier_of(info) == identifier_of(TraitMethod);
               }) |
               stdr::to<vector<info>>();
    };
    auto ret = array<info, get_vec().size()>();
    stdr::copy(get_vec(), ret.begin());
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

template<typename Impl, std::meta::info TraitMethod>
consteval bool has_method() {
    using namespace std;
    using namespace std::meta;
    constexpr auto impl_mems    = matching_id_methods<Impl, TraitMethod>();
    constexpr auto make_matcher = [](info impl_method) {
        auto match_targs = vector{^^Impl, reflect_constant(impl_method), return_type_of(TraitMethod)};
        match_targs.append_range(parameters_of(TraitMethod) | stdv::transform(type_of));
        return substitute(^^match_method, match_targs);
    };
    auto [... Is] = make_Is<impl_mems.size()>();
    return ([:make_matcher(impl_mems[Is]):]() || ...);
};

template<typename Impl, typename Trait, uZ I>
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
