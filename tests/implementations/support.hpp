#pragma once

#include "support/value_support.hpp"

using value_trait = valuetest::mutable_trait;

template<typename Base, int>
struct derived : Base {};

template<typename Base, int>
struct virtual_derived : virtual Base {};
