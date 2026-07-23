#include "trait_variant/adapters.hpp"
#include "trait_variant/aliasing.hpp"
#include "trait_variant/basic.hpp"
#include "trait_variant/constraints.hpp"
#include "trait_variant/lifecycle.hpp"
#include "trait_variant/reference_qualification.hpp"

int main() {
    basic::run();
    constraints::run();
    lifecycle::run();
    aliasing::run();
    adapters::run();
    reference_qualification::run();
}
