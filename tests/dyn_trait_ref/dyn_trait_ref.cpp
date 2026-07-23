#include "dyn_trait_ref/adapters.hpp"
#include "dyn_trait_ref/casts.hpp"
#include "dyn_trait_ref/const_casts.hpp"
#include "dyn_trait_ref/cv_qualification.hpp"
#include "dyn_trait_ref/forwarding.hpp"

int main() {
    casts::run();
    const_casts::run();
    cv_qualification::run();
    forwarding::run();
    adapters::run();
}
