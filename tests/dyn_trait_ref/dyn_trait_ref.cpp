#include "dyn_trait_ref/adapters.hpp"
#include "dyn_trait_ref/casts.hpp"
#include "dyn_trait_ref/cv_qualification.hpp"

int main() {
    casts::run();
    cv_qualification::run();
    adapters::run();
}
