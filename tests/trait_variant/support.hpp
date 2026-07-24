#pragma once

#include "support/value_support.hpp"
#include "trp/trait_variant.hpp"

using value_trait = valuetest::trait;

template<int Tag>
using value_impl = valuetest::value_impl<Tag>;

template<int Tag>
using marker = valuetest::marker<Tag>;

using stable_value_impl   = valuetest::value_impl<0>;
using throwing_value_impl = valuetest::throwing_impl;
using valueless_variant_t = trp::trait_variant<value_trait, stable_value_impl, throwing_value_impl>;
