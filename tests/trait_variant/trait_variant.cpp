#include "trait_variant/adapters.hpp"
#include "trait_variant/alignment.hpp"
#include "trait_variant/assignment.hpp"
#include "trait_variant/basic.hpp"
#include "trait_variant/constraints.hpp"
#include "trait_variant/forwarding.hpp"
#include "trait_variant/lifecycle.hpp"
#include "trait_variant/reference_qualification.hpp"
#include "trait_variant/valueless.hpp"

int main() {
    basic::run();
    constraints::run();
    lifecycle::run();
    adapters::run();
    reference_qualification::run();
    forwarding::run();
    assignment::run();
    valueless::run();
    alignment::run();
}
