#pragma once

#include <cstdlib>
#include <format>
#include <meta>
#include <print>
#include <string_view>
#include <type_traits>

namespace test {

[[noreturn]] inline void fail(std::string_view context, std::string_view detail) {
    std::println(stderr, "{}: {}", context, detail);
    std::abort();
}

template<typename Enum>
    requires std::is_enum_v<Enum>
inline constexpr auto all_enum_enumerators = std::define_static_array(std::meta::enumerators_of(^^Enum));

template<typename Enum>
    requires std::is_enum_v<Enum>
constexpr auto enum_name(Enum value) -> std::string_view {
    template for (constexpr auto enumerator: all_enum_enumerators<Enum>) {
        if (value == [:enumerator:])
            return std::meta::identifier_of(enumerator);
    }
    return "<unknown>";
}

template<typename Enum>
struct enum_formatter : std::formatter<std::string_view> {
    auto format(Enum value, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format(enum_name(value), ctx);
    }
};

template<typename T>
inline void expect_eq(std::string_view case_name, std::string_view check_name, T actual, T expected) {
    if (actual == expected)
        return;

    std::println(stderr, "{} [{}]: expected {}, got {}", case_name, check_name, expected, actual);
    std::abort();
}

}    // namespace test
