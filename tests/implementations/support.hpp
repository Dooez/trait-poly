#pragma once

struct value_trait {
    auto value() -> int;
};

template<typename Base, int>
struct derived : Base {};

template<typename Base, int>
struct virtual_derived : virtual Base {};
