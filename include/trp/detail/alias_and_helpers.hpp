#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <meta>
#include <ranges>
#include <type_traits>


#if defined(__clang__)
namespace std::meta {
consteval bool is_vararg_function(meta::info r) {
    return is_function(r) and has_ellipsis_parameter(r);
}
}    // namespace std::meta
#endif

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

template<typename T, meta::reflection_range R = std::initializer_list<meta::info>>
consteval auto subextract_span(meta::info r, R const& targs) -> std::span<T const> {
    auto const src_span = meta::reflect_constant(meta::substitute(r, targs));
    return std::span<T const>{extract<T const*>(substitute(^^extract_ptr, {src_span})),
                              extract<uZ>(substitute(^^extract_size, {src_span}))};
}

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

    [[nodiscard]] consteval auto is_cv() const {
        return is_const and is_volatile;
    }
    [[nodiscard]] consteval auto is_ref() const {
        return is_lvalue or is_rvalue;
    }
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

    using return_type = Ret;
    static constexpr auto param_infos =
        std::define_static_array(std::array<meta::info, sizeof...(Params)>{^^Params...});
    static constexpr auto param_identities = make_aggregate(std::type_identity<Params>{}...);

    static consteval auto add_obj_cvref(meta::info inf) {
        if (is_lvalue_reference_type(inf) or is_rvalue_reference_type(inf))
            throw "Input must be non-ref";
        inf = remove_reference(inf);
        if (is_volatile)
            inf = add_volatile(inf);
        if (is_const)
            inf = add_const(inf);
        if (is_lvalue)
            inf = add_lvalue_reference(inf);
        if (is_rvalue)
            inf = add_rvalue_reference(inf);
        return inf;
    };

    static consteval auto add_obj_call_cvref(meta::info inf) {
        inf = add_obj_cv(inf);
        if (is_rvalue)
            return add_rvalue_reference(inf);
        return add_lvalue_reference(inf);
    }

    static consteval auto add_obj_cv(meta::info inf) {
        auto const is_lref_obj = is_lvalue_reference_type(inf);
        auto const is_rref_obj = is_rvalue_reference_type(inf);

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
    template<typename T>
    using as_obj = [:add_obj_cv(^^T):];

    using wrapper_fptr_type = auto (*)(void*, Params...) noexcept(is_noexcept) -> Ret;
};

template<typename Ret, bool Noexcept, typename... Params>
using function_ptr_t = auto (*)(Params...) noexcept(Noexcept) -> Ret;

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
#undef TRP_FQUAL_FPTR_NON_EOP
}():];

consteval auto method_identity(meta::info method_info) -> meta::info {
    auto const is_nsmf = is_function(method_info)                 //
                         and not is_static_member(method_info)    //
                         and not is_virtual(method_info)          //
                         and has_identifier(method_info);
    if (not is_nsmf) {
        throw "get_method_identity() can only be called on non-static non-template "
              "member functions with identifiers";
    }


    auto quals            = method_qualifiers_t{};
    quals.is_noexcept     = is_noexcept(method_info);
    auto const identifier = meta::reflect_constant_string(identifier_of(method_info));
    auto const ret        = return_type_of(method_info);
    auto const params     = parameters_of(method_info);
    if (not params.empty() and is_explicit_object_parameter(params[0])) {
        auto const eop_t  = type_of(params[0]);
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
template<typename MethodIdt>
inline constexpr char const* identifier_of_method = MethodIdt::identifier;

consteval auto extract_method_identifier(meta::info idt) -> char const* {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    // return extract<char const*>(template_arguments_of(idt)[0]); // clang can't handle
    return extract<char const*>(substitute(^^identifier_of_method, {idt}));
}

consteval auto extract_method_qualifiers(meta::info idt) -> method_qualifiers_t {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    return extract<method_qualifiers_t>(template_arguments_of(idt)[1]);
}

consteval auto extract_method_param_types(meta::info idt) -> std::span<meta::info const> {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    return std::define_static_array(template_arguments_of(idt) | stdv::drop(3));
}
consteval auto extract_method_return_type(meta::info idt) -> meta::info {
    if (not has_template_arguments(idt) or template_of(idt) != ^^method_identity_t)
        throw "Expected method_identity_t specialization";
    return template_arguments_of(idt)[2];
}

consteval auto add_method_obj_cv(meta::info method, meta::info inf) {
    auto const quals       = extract_method_qualifiers(method);
    auto const is_lref_obj = is_lvalue_reference_type(inf);
    auto const is_rref_obj = is_rvalue_reference_type(inf);

    inf = remove_reference(inf);
    if (quals.is_volatile)
        inf = add_volatile(inf);
    if (quals.is_const)
        inf = add_const(inf);
    if (is_lref_obj)
        inf = add_lvalue_reference(inf);
    if (is_rref_obj)
        inf = add_rvalue_reference(inf);
    return inf;
}
consteval auto add_method_obj_cvref(meta::info method, meta::info inf) {
    auto const quals = extract_method_qualifiers(method);
    if (is_lvalue_reference_type(inf) or is_rvalue_reference_type(inf))
        throw "Input must be non-ref";
    inf = remove_reference(inf);
    if (quals.is_volatile)
        inf = add_volatile(inf);
    if (quals.is_const)
        inf = add_const(inf);
    if (quals.is_lvalue)
        inf = add_lvalue_reference(inf);
    if (quals.is_rvalue)
        inf = add_rvalue_reference(inf);
    return inf;
};

consteval auto replace_method_qualifiers(meta::info method, method_qualifiers_t quals) -> meta::info {
    auto upd_targs = std::vector{meta::reflect_constant(extract_method_identifier(method)),
                                 meta::reflect_constant(quals),
                                 extract_method_return_type(method)};
    upd_targs.append_range(extract_method_param_types(method));
    return substitute(^^method_identity_t, upd_targs);
}

template<typename T>
concept any_method_idt = has_template_arguments(^^T) and template_of(^^T) == ^^method_identity_t;

template<typename T>
concept trait_method_idt = any_method_idt<T> and not(T::is_value) and std::string_view(T::identifier) != "_";

consteval bool is_const_idt(meta::info idt) {
    return extract_method_qualifiers(idt).is_const;
}
consteval bool is_volatile_idt(meta::info idt) {
    return extract_method_qualifiers(idt).is_volatile;
}
consteval bool is_cv_idt(meta::info idt) {
    return extract_method_qualifiers(idt).is_const and extract_method_qualifiers(idt).is_volatile;
}

consteval bool are_similar_idts(meta::info a, meta::info b) {
    if (std::string_view(extract_method_identifier(a)) != extract_method_identifier(b))
        return false;

    auto const qa = extract_method_qualifiers(a);
    auto const qb = extract_method_qualifiers(b);
    if (qa.is_const != qb.is_const or qa.is_volatile != qb.is_volatile)
        return false;

    auto const parama = extract_method_param_types(a);
    auto const paramb = extract_method_param_types(b);
    if (not stdr::equal(parama, paramb))
        return false;

    auto const reta = extract_method_return_type(a);
    auto const retb = extract_method_return_type(b);
    if (reta != retb)
        throw "methods differ by return type only";
    return true;
}

/**
 * @brief checks if method identity range contains a potentially more specialized version of method_idt
 */
consteval bool contains_submethod_of(std::span<meta::info const> idt_range, meta::info method_idt) {
    auto const quals = extract_method_qualifiers(method_idt);
    auto const ret   = extract_method_return_type(method_idt);

    auto has_lvalue = false;
    auto has_rvalue = false;
    for (auto const candidate: idt_range) {
        if (extract_method_return_type(candidate) != ret)
            continue;
        if (not are_similar_idts(candidate, method_idt))
            continue;
        auto const candidate_quals = extract_method_qualifiers(candidate);
        if (quals.is_noexcept and not candidate_quals.is_noexcept)
            continue;
        if (not candidate_quals.is_ref())
            return true;
        has_lvalue |= candidate_quals.is_lvalue;
        has_rvalue |= candidate_quals.is_rvalue;

        if (has_lvalue and has_rvalue)
            return true;
    }
    if (quals.is_lvalue)
        return has_lvalue;
    if (quals.is_rvalue)
        return has_rvalue;
    return has_lvalue and has_rvalue;
}

consteval auto copy_cv_to(meta::info proto, meta::info type) {
    if (is_const(proto))
        type = add_const(type);
    if (is_volatile(proto))
        type = add_volatile(type);
    return type;
}
consteval auto copy_cvref_to(meta::info proto, meta::info type) {
    if (is_lvalue_reference_type(type) or is_rvalue_reference_type(type))
        throw "Non-reference type expected";
    if (is_const(remove_reference(proto)))
        type = add_const(type);
    if (is_volatile(remove_reference(proto)))
        type = add_volatile(type);
    if (is_lvalue_reference_type(proto))
        type = add_lvalue_reference(type);
    if (is_rvalue_reference_type(proto))
        type = add_rvalue_reference(type);

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
#ifndef TRP_DEFAULT_MATCH_METHOD_ARGS
    static_assert(false,
                  "Exact cv- qualification requirements can only be enabled together with exact arguments "
                  "requirements.");
#endif
#else
        false;
#endif
    bool exact_ref =
#ifdef TRP_DEFAULT_MATCH_METHOD_REF
        true;
#ifndef TRP_DEFAULT_MATCH_METHOD_ARGS
    static_assert(false,
                  "Exact reference qualification requirements can only be enabled together with exact "
                  "argument requirements.");
#endif
#else
        false;
#endif

    friend consteval void swap(method_signature_requirements_t& lhs, method_signature_requirements_t& rhs) {
        std::swap(lhs.exact_return, rhs.exact_return);
        std::swap(lhs.exact_args, rhs.exact_args);
        std::swap(lhs.exact_cv, rhs.exact_cv);
        std::swap(lhs.exact_ref, rhs.exact_ref);
    }

    consteval auto operator|=(method_signature_requirements_t const& other) {
        exact_return |= other.exact_return;
        exact_args |= other.exact_args;
        exact_cv |= other.exact_cv;
        exact_ref |= other.exact_ref;
        return *this;
    }
};
consteval auto extract_signature_req(meta::info r) -> std::optional<method_signature_requirements_t> {
    auto const annotations =
        annotations_of(r)                                                                                 /**/
        | stdv::filter([](auto r) { return remove_cv(type_of(r)) == ^^method_signature_requirements_t; }) /**/
        | stdr::to<std::vector>();
    if (annotations.size() > 1)
        throw "More than one method requirements annotation.";
    if (annotations.empty())
        return std::nullopt;
    return extract<method_signature_requirements_t>(constant_of(annotations.front()));
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
        auto const verify = [](auto const& req) {
            if (req.exact_cv and not req.exact_args)
                throw "Exact cv signature requirement must include exact arguments requirement";
            if (req.exact_ref and not req.exact_args)
                throw "Exact reference signature requirement must include exact arguments requirement";
            return req;
        };
        if (auto o_req = extract_signature_req(trait_member))
            return verify(*o_req);
        auto const trait = parent_of(trait_member);
        if (auto o_req = extract_signature_req(trait))
            return verify(*o_req);
        return {};
    };
};

consteval auto direct_trait_methods_and_requirements_fn(meta::info trait) {
    auto is_relevant_method = [=](meta::info method) {
        return is_function(method)                              //
               and not is_special_member_function(method)       //
               and (is_const(method) or not is_const(trait))    //
               and (is_volatile(method) or not is_volatile(trait));
    };

    auto const relevant_methods = members_of(trait, unprivileged)       //
                                  | stdv::filter(is_relevant_method)    //
                                  | stdr::to<std::vector>();
    return methods_and_requirements{
        .identities   = define_static_array(relevant_methods | stdv::transform(method_identity)),
        .requirements = define_static_array(relevant_methods    //
                                            | stdv::transform(methods_and_requirements::to_o_requirements)),
    };
};

template<typename T>
inline constexpr auto direct_trait_methods_and_requirements = direct_trait_methods_and_requirements_fn(^^T);
template<typename T>
inline constexpr auto direct_trait_methods = direct_trait_methods_and_requirements<T>.identities;
template<typename T>
inline constexpr auto direct_trait_requirements = direct_trait_methods_and_requirements<T>.requirements;

consteval auto all_trait_methods_and_requirements_fn(meta::info trait) -> methods_and_requirements;
template<typename T>
inline constexpr auto all_trait_methods_and_requirements = all_trait_methods_and_requirements_fn(^^T);
/**
 * @brief All reflections of method_identity_t for all methods in a trait T sorted in lexicographical order of identifiers
 * @tparam T 
 */
template<typename T>
inline constexpr auto all_trait_methods = all_trait_methods_and_requirements<T>.identities;
template<typename T>
inline constexpr auto all_trait_requirements = all_trait_methods_and_requirements<T>.requirements;

namespace atmar {
using sign_req_t = method_signature_requirements_t;
consteval void update_at(uZ                       i,
                         auto const&              quals,
                         auto const&              reqs,
                         std::vector<meta::info>& res_idts,
                         std::vector<sign_req_t>& res_reqs) {
    auto curr_quals = extract_method_qualifiers(res_idts[i]);
    curr_quals.is_noexcept |= quals.is_noexcept;
    curr_quals.is_lvalue |= quals.is_lvalue;
    curr_quals.is_rvalue |= quals.is_rvalue;

    res_idts[i] = replace_method_qualifiers(res_idts[i], curr_quals);
    res_reqs[i] |= reqs;
}
consteval void append_as_ref(meta::info               method,
                             auto const&              reqs,
                             bool                     is_lvalue,
                             std::vector<meta::info>& res_idts,
                             std::vector<sign_req_t>& res_reqs) {
    auto quals      = extract_method_qualifiers(method);
    quals.is_lvalue = is_lvalue;
    quals.is_rvalue = not is_lvalue;
    res_idts.push_back(replace_method_qualifiers(method, quals));
    res_reqs.push_back(reqs);
}
consteval void append_unique(std::span<meta::info const> idts,
                             std::span<sign_req_t const> reqs,
                             std::vector<meta::info>&    res_idts,
                             std::vector<sign_req_t>&    res_reqs) {
    for (auto i: stdv::iota(0U, idts.size())) {
        auto const method       = idts[i];
        auto const requirements = reqs[i];
        auto const size         = res_idts.size();

        auto unqual_idx = size;
        auto lvalue_idx = size;
        auto rvalue_idx = size;
        for (auto i: stdv::iota(0UZ, size)) {
            if (not are_similar_idts(res_idts[i], method))
                continue;
            auto const quals = extract_method_qualifiers(res_idts[i]);
            if (quals.is_lvalue) {
                lvalue_idx = i;
            } else if (quals.is_rvalue) {
                rvalue_idx = i;
                break;
            } else {
                unqual_idx = i;
                break;
            }
        }

        if (unqual_idx == size     /**/
            and lvalue_idx == size /**/
            and rvalue_idx == size) {
            res_idts.push_back(method);
            res_reqs.push_back(requirements);
            continue;
        }

        auto const method_quals = extract_method_qualifiers(method);
        if (unqual_idx != size) {
            if (not method_quals.is_ref()) {
                update_at(unqual_idx, method_quals, requirements, res_idts, res_reqs);
                continue;
            }
            auto const current_method = res_idts[unqual_idx];

            auto lvalue_req   = res_reqs[unqual_idx];
            auto lvalue_quals = extract_method_qualifiers(current_method);
            auto rvalue_req   = lvalue_req;
            auto rvalue_quals = lvalue_quals;

            if (method_quals.is_lvalue)
                lvalue_req |= requirements;
            else
                rvalue_req |= requirements;

            lvalue_quals.is_lvalue = true;
            if (method_quals.is_lvalue)
                lvalue_quals.is_noexcept |= method_quals.is_noexcept;
            update_at(unqual_idx, lvalue_quals, lvalue_req, res_idts, res_reqs);

            rvalue_quals.is_rvalue = true;
            if (method_quals.is_rvalue)
                rvalue_quals.is_noexcept |= method_quals.is_noexcept;
            res_idts.push_back(replace_method_qualifiers(current_method, rvalue_quals));
            res_reqs.push_back(rvalue_req);
            continue;
        }

        if (method_quals.is_lvalue) {
            if (lvalue_idx == size)
                append_as_ref(method, requirements, true, res_idts, res_reqs);
            else
                update_at(lvalue_idx, method_quals, requirements, res_idts, res_reqs);
        } else if (method_quals.is_rvalue) {
            if (rvalue_idx == size)
                append_as_ref(method, requirements, false, res_idts, res_reqs);
            else
                update_at(rvalue_idx, method_quals, requirements, res_idts, res_reqs);
        } else {
            if (lvalue_idx == size)
                append_as_ref(method, requirements, true, res_idts, res_reqs);
            else
                update_at(lvalue_idx, method_quals, requirements, res_idts, res_reqs);
            if (rvalue_idx == size)
                append_as_ref(method, requirements, false, res_idts, res_reqs);
            else
                update_at(rvalue_idx, method_quals, requirements, res_idts, res_reqs);
        }
    }
}

consteval auto method_idt_less(meta::info idt_l, meta::info idt_r) -> bool {
    auto const id_l = std::string_view(extract_method_identifier(idt_l));
    auto const id_r = std::string_view(extract_method_identifier(idt_r));
    if (id_l != id_r)
        return id_l < id_r;
    auto const quals_l  = extract_method_qualifiers(idt_l);
    auto const quals_r  = extract_method_qualifiers(idt_r);
    auto const ref_rank = [](method_qualifiers_t quals) {
        if (quals.is_lvalue)
            return 0;
        if (quals.is_rvalue)
            return 2;
        return 1;
    };
    return ref_rank(quals_l) < ref_rank(quals_r);
}
}    // namespace atmar

consteval auto all_trait_methods_and_requirements_fn(meta::info trait) -> methods_and_requirements {
    using namespace atmar;
    auto res_idts = subextract_info_span(^^direct_trait_methods, {trait})    //
                    | stdr::to<std::vector>();
    auto res_reqs = subextract_span<sign_req_t>(^^direct_trait_requirements, {trait})    //
                    | stdr::to<std::vector>();

    for (auto base: subextract_base_types(remove_cv(trait))) {
        auto const cv_base = copy_cv_to(trait, base);
        auto const idts    = subextract_info_span(^^all_trait_methods, {cv_base});
        auto const reqs    = subextract_span<sign_req_t>(^^all_trait_requirements, {cv_base});
        append_unique(idts, reqs, res_idts, res_reqs);
    }
    uZ const end = res_idts.size();
    for (auto unsorted_end = end; unsorted_end > 1; --unsorted_end) {
        auto swapped = false;
        for (auto i: stdv::iota(0UZ, unsorted_end - 1)) {
            if (method_idt_less(res_idts[i + 1], res_idts[i])) {
                std::swap(res_idts[i], res_idts[i + 1]);
                swap(res_reqs[i], res_reqs[i + 1]);
                swapped = true;
            }
        }
        if (not swapped)
            break;
    }
    return methods_and_requirements{
        .identities   = std::define_static_array(res_idts),
        .requirements = std::define_static_array(res_reqs),
    };
};


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
consteval void expect(meta::info check, std::initializer_list<meta::info> args) {
    if (not extract<bool>(substitute(check, args)))
        throw "unexpexted error";
}

}    // namespace detail
}    // namespace trp
