#pragma once
#include <algorithm>
#include <concepts>
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

static constexpr auto unprivileged = meta::access_context::unprivileged();

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

struct method_qualifiers_t {
    bool is_const;
    bool is_volatile;
    bool is_lvalue;
    bool is_rvalue;
    bool is_value;
    bool is_noexcept;
};
template<method_qualifiers_t quals>
concept valid_quals = (quals.is_lvalue + quals.is_rvalue + quals.is_value) <= 1    //
                      and (not quals.is_value or not quals.is_volatile);

template<char const* Identifier, method_qualifiers_t Quals, typename Ret, typename... Params>
    requires valid_quals<Quals>
struct method_identity_t {
    static constexpr auto identifier  = Identifier;
    static constexpr bool is_const    = Quals.is_const;
    static constexpr bool is_volatile = Quals.is_volatile;
    static constexpr bool is_cv       = is_const and is_volatile;
    static constexpr bool is_lvalue   = Quals.is_lvalue;
    static constexpr bool is_rvalue   = Quals.is_rvalue;
    static constexpr bool is_value    = Quals.is_value;
    static constexpr bool is_noexcept = Quals.is_noexcept;
    static constexpr auto qualifiers  = Quals;

    using return_type                      = Ret;
    static constexpr auto param_infos      = std::array<meta::info, sizeof...(Params)>{^^Params...};
    static constexpr auto param_identities = make_aggregate(std::type_identity<Params>{}...);

    static consteval auto add_obj_cv(meta::info inf) {
        auto const is_lref_obj = meta::is_lvalue_reference_type(inf);
        auto const is_rref_obj = meta::is_rvalue_reference_type(inf);

        inf = remove_reference(inf);
        if (is_volatile)
            inf = add_volatile(inf);
        if (is_const)
            inf = add_const(inf);
        if (is_lref_obj)
            inf = add_lvalue_reference(inf);
        if (is_rref_obj)
            inf = add_rvalue_reference(inf);
        return inf;
    };
    using wrapper_fptr_type = auto (*)(void*, Params...) noexcept(is_noexcept) -> Ret;
};

template<bool EOP, typename Impl, typename Ret, bool Noexcept, typename... Params>
using make_function_member_type = [:[] {
    constexpr auto is_const    = meta::is_const(remove_reference(^^Impl));
    constexpr auto is_volatile = meta::is_volatile(remove_reference(^^Impl));
    constexpr auto is_rvalue   = is_rvalue_reference_type(^^Impl);
    constexpr auto is_lvalue   = is_lvalue_reference_type(^^Impl);
#define TRP_FQUAL_FPTR(C, V, R)                                                                           \
    {                                                                                                     \
        if constexpr (EOP) {                                                                              \
            using mfptr_t = auto (*)(std::remove_cvref_t<Impl> C V R, Params...) noexcept(Noexcept)->Ret; \
            return dealias(^^mfptr_t);                                                                    \
        }                                                                                                 \
        using mfptr_t = auto (std::remove_cvref_t<Impl>::*)(Params...) C V R noexcept(Noexcept)->Ret;     \
        return dealias(^^mfptr_t);                                                                        \
    }
#define TRP_FQUAL_FPTR_NON_EOP(C, V, R)                                                               \
    {                                                                                                 \
        if constexpr (EOP) {                                                                          \
            throw "Volatile value parameter is deprecated";                                           \
        }                                                                                             \
        using mfptr_t = auto (std::remove_cvref_t<Impl>::*)(Params...) C V R noexcept(Noexcept)->Ret; \
        return dealias(^^mfptr_t);                                                                    \
    }

    if (is_const and is_volatile) {
        if (is_rvalue)
            TRP_FQUAL_FPTR(const, volatile, &&)
        if (is_lvalue)
            TRP_FQUAL_FPTR(const, volatile, &)
        TRP_FQUAL_FPTR_NON_EOP(const, volatile, )
    }
    if (is_const) {
        if (is_rvalue)
            TRP_FQUAL_FPTR(const, , &&)
        if (is_lvalue)
            TRP_FQUAL_FPTR(const, , &)
        TRP_FQUAL_FPTR(const, , )
    }
    if (is_volatile) {
        if (is_rvalue)
            TRP_FQUAL_FPTR(, volatile, &&)
        if (is_lvalue)
            TRP_FQUAL_FPTR(, volatile, &)
        TRP_FQUAL_FPTR_NON_EOP(, volatile, )
    }
    if (is_rvalue)
        TRP_FQUAL_FPTR(, , &&)
    if (is_lvalue)
        TRP_FQUAL_FPTR(, , &)
    TRP_FQUAL_FPTR(, , )
#undef TRP_FQUAL_FPTR
#undef TRP_FQUAL_FPTR_NONEOP
}    // namespace detail
                                   ():];

consteval auto method_identity(meta::info method_info) -> meta::info {
    auto is_nsmf = is_function(method_info)                 //
                   and not is_static_member(method_info)    //
                   and not is_virtual(method_info)          //
                   and has_identifier(method_info);
    if (not is_nsmf) {
        throw "get_method_identity() can only be called on non-static non-template "
              "member functions with identifiers";
    }


    auto       quals      = method_qualifiers_t{};
    auto const identifier = meta::reflect_constant_string(identifier_of(method_info));
    quals.is_noexcept     = is_noexcept(method_info);
    auto const ret        = return_type_of(method_info);
    auto       params     = parameters_of(method_info);
    if (not params.empty() and is_explicit_object_parameter(params[0])) {
        auto const eop    = params[0];
        auto const eop_t  = type_of(eop);
        quals.is_const    = is_const(remove_reference(eop_t));
        quals.is_volatile = is_volatile(remove_reference(eop_t));
        quals.is_lvalue   = is_lvalue_reference_type(eop_t);
        quals.is_rvalue   = is_rvalue_reference_type(eop_t);
        quals.is_value    = not is_lvalue_reference_type(eop_t)    //
                         and not is_rvalue_reference_type(eop_t);
        auto arguments = std::vector{identifier,    //
                                     meta::reflect_constant(quals),
                                     ret};
        arguments.append_range(params | stdv::drop(1) | stdv::transform(meta::type_of));
        return substitute(^^method_identity_t, arguments);
    }
    quals.is_const    = is_const(method_info);
    quals.is_volatile = is_volatile(method_info);
    quals.is_lvalue   = is_lvalue_reference_qualified(method_info);
    quals.is_rvalue   = is_rvalue_reference_qualified(method_info);
    quals.is_value    = false;
    auto arguments    = std::vector{identifier,    //
                                 meta::reflect_constant(quals),
                                 ret};
    arguments.append_range(params | stdv::transform(meta::type_of));
    return substitute(^^method_identity_t, arguments);
}
template<typename MethodIdt>
inline constexpr char const* method_identifier = MethodIdt::identifier;

template<typename MethodIdt>
inline constexpr auto method_qualifiers = MethodIdt::qualifiers;

template<typename MethodIdt>
inline constexpr auto method_params = std::span<meta::info const>(MethodIdt::param_infos);


consteval auto as_lref_method_identity(meta::info idt) -> meta::info {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    auto targs = template_arguments_of(idt);
    auto quals = extract<method_qualifiers_t>(targs[1]);
    if (quals.is_rvalue or quals.is_lvalue or quals.is_value)
        return idt;
    quals.is_lvalue = true;
    targs[1]        = meta::reflect_constant(quals);
    return substitute(^^method_identity_t, targs);
}
consteval auto extract_method_identifier(meta::info idt) -> char const* {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    return extract<char const*>(substitute(^^method_identifier, {idt}));
};
consteval auto extract_method_qualifiers(meta::info idt) -> method_qualifiers_t {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    return extract<method_qualifiers_t>(substitute(^^method_qualifiers, {idt}));
};
consteval auto extract_method_params(meta::info idt) -> std::span<meta::info const> {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    return subextract_info_span(^^method_params, {idt});
};

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

struct method_signature_requirements_t {
    bool exact_return =
#ifdef TRP_DEFAULT_MATCH_METHOD_RETURN
        true;
#else
        false;
#endif
    bool exact_args =
#ifdef TRP_DEFAULT_MATCH_METHOD_ARGS
        true;
#else
        false;
#endif
    bool exact_cv =
#ifdef TRP_DEFAULT_MATCH_METHOD_CV
        true;
#else
        false;
#endif

    consteval auto operator|=(const method_signature_requirements_t& other) {
        exact_return |= other.exact_return;
        exact_args |= other.exact_args;
        exact_cv |= other.exact_cv;
        return *this;
    }
};
consteval auto extract_signature_req(meta::info r) -> std::optional<method_signature_requirements_t> {
    auto const annotations =
        annotations_of(r)                                                                         //
        | stdv::filter([](auto r) { return type_of(r) == ^^method_signature_requirements_t; })    //
        | stdr::to<std::vector>();
    if (annotations.size() > 1)
        throw "More than one method requirements annoation.";
    if (annotations.empty())
        return std::nullopt;
    return extract<method_signature_requirements_t>(object_of(annotations.front()));
}

template<non_cvref T>
inline constexpr auto nonspecial_members = std::define_static_array(
    members_of(^^T, unprivileged) | stdv::filter(std::not_fn(meta::is_special_member_function)));

template<non_cvref T>
inline constexpr auto direct_base_types =
    std::define_static_array(bases_of(^^T, unprivileged) | stdv::transform(meta::type_of));

consteval auto subextract_base_types(meta::info type) {
    return subextract_info_span(^^direct_base_types, {type});
}

struct methods_and_requirements {
    std::span<meta::info const>                      identities;
    std::span<method_signature_requirements_t const> requirements;

    static consteval auto to_o_requirements(meta::info trait_member) -> method_signature_requirements_t {
        if (auto o_req = extract_signature_req(trait_member))
            return *o_req;
        auto const trait = parent_of(trait_member);
        if (auto o_req = extract_signature_req(trait))
            return *o_req;
        return {};
    };

    consteval auto zip() const {
        return stdv::zip(identities, requirements);
    }
};

template<typename T>
inline constexpr auto direct_trait_methods_and_requirements = [] {
    constexpr auto is_relevant_method = [](meta::info method) {
        return is_function(method)                            //
               and not is_special_member_function(method)     //
               and (is_const(method) or not is_const(^^T))    //
               and (is_volatile(method) or not is_volatile(^^T));
    };

    auto const relevant_methods = members_of(^^T, unprivileged)         //
                                  | stdv::filter(is_relevant_method)    //
                                  | stdr::to<std::vector>();
    return methods_and_requirements{
        .identities   = define_static_array(relevant_methods | stdv::transform(method_identity)),
        .requirements = define_static_array(relevant_methods    //
                                            | stdv::transform(methods_and_requirements::to_o_requirements)),
    };
}();

template<typename T>
inline constexpr auto all_trait_methods_and_requirements = [] {
    auto res_idts = direct_trait_methods_and_requirements<T>.identities    //
                    | stdr::to<std::vector>();
    auto res_reqs = direct_trait_methods_and_requirements<T>.requirements    //
                    | stdr::to<std::vector>();

    auto const append_unique = [&](auto&& idts, auto&& reqs) {
        for (auto [m, req]: stdv::zip(idts, reqs)) {
            auto const it = stdr::find(res_idts, m);
            if (it == res_idts.end()) {
                res_idts.push_back(m);
                res_reqs.push_back(req);
                continue;
            }
            auto const i = stdr::distance(res_idts.begin(), it);
            res_reqs[i] |= req;
        }
    };
    template for (constexpr auto base: direct_base_types<std::remove_cv_t<T>>) {
        using base_t = [:copy_cv_to(^^T, base):];
        append_unique(all_trait_methods_and_requirements<base_t>.identities,
                      all_trait_methods_and_requirements<base_t>.requirements);
    }
    auto const method_id_less = [](auto&& zip_v_l, auto&& zip_v_r) {
        auto [idt_l, _] = zip_v_l;
        auto [idt_r, _] = zip_v_r;
        return std::string_view(extract_method_identifier(idt_l)) <
               std::string_view(extract_method_identifier(idt_r));
    };
    stdr::sort(stdv::zip(res_idts, res_reqs), method_id_less);
    return methods_and_requirements{
        .identities   = std::define_static_array(res_idts),
        .requirements = std::define_static_array(res_reqs),
    };
}();

template<typename T>
inline constexpr auto direct_trait_methods = direct_trait_methods_and_requirements<T>.identities;

/**
 * @brief All reflections of method_identity_t for all methods in a trait T sorted in lexicographical order of identifiers
 * @tparam T 
 */
template<typename T>
inline constexpr auto all_trait_methods = all_trait_methods_and_requirements<T>.identities;

struct method_reference {
    char const* name;         // method identifier
    uZ          begin_idx;    // index in the `all_trait_methods<T>` of the first method in a group
    uZ          end_idx;    // one past the index in the `all_trait_methods<T>` of the last method in a group
};

template<typename T>
inline constexpr auto trait_method_groups = [] -> std::span<method_reference const> {
    if (stdr::empty(all_trait_methods<T>))
        return {};
    auto groups = std::vector<method_reference>{};
    groups.push_back({
        .name      = extract_method_identifier(all_trait_methods<T>[0]),
        .begin_idx = 0,
    });
    for (auto [i, idt]: stdv::zip(stdv::iota(0U), all_trait_methods<T>) | stdv::drop(1)) {
        if (std::string_view name = extract_method_identifier(idt); name != groups.back().name) {
            groups.back().end_idx = i;
            groups.push_back({
                .name      = name.data(),
                .begin_idx = i,
                .end_idx   = i + 1,
            });
        }
    }
    groups.back().end_idx = all_trait_methods<T>.size();
    return std::define_static_array(groups);
}();

template<non_ref T>
struct unique_id_struct {
    inline static char value{};
};

template<typename T>
consteval auto find_annotated_member(meta::info           type,
                                     T                    annotation,
                                     meta::access_context ctx = meta::access_context::current())
    -> meta::info {
    auto const target_ann = meta::reflect_constant(annotation);
    for (auto m: nonstatic_data_members_of(type, ctx)) {
        for (auto const ann: annotations_of(m) | stdv::transform(meta::constant_of))
            if (ann == target_ann)
                return m;
        for (auto const ann: annotations_of(type_of(m)) | stdv::transform(meta::constant_of))
            if (ann == target_ann)
                return m;
    }
    for (auto base: subextract_base_types(type)) {
        auto m = find_annotated_member(base, annotation, ctx);
        if (m != meta::info{})
            return m;
    }
    return meta::info{};
}

template<typename T>
consteval auto find_annotated_base(meta::info           type,
                                   T                    annotation,
                                   meta::access_context ctx = meta::access_context::current()) -> meta::info {
    auto const target_ann = meta::reflect_constant(annotation);
    auto const bases      = subextract_base_types(type);
    for (auto const base: bases) {
        for (auto const ann: annotations_of(base) | stdv::transform(meta::constant_of))
            if (ann == target_ann)
                return base;
        for (auto const ann: annotations_of(type_of(base)) | stdv::transform(meta::constant_of))
            if (ann == target_ann)
                return base;
    }
    for (auto const base: bases) {
        auto const m = find_annotated_base(base, annotation);
        if (m != meta::info{})
            return m;
    }
    return meta::info{};
}
}    // namespace detail
}    // namespace trp
