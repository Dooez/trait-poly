// Exact reference matching is only valid together with exact argument matching.
#define TRP_DEFAULT_MATCH_METHOD_ARGS
#define TRP_DEFAULT_MATCH_METHOD_REF

#include "references/adapters.hpp"
#include "requirements/reference_qualification.hpp"

int main() {
    adapters::run();
}
