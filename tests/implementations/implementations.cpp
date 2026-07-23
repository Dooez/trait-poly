#include "implementations/callable_objects.hpp"
#include "implementations/inherited_explicit.hpp"
#include "implementations/inherited_ref_vtable.hpp"
#include "implementations/inherited_virtual.hpp"
#include "implementations/static_methods.hpp"

#if defined(TRP_TEST_STATIC_TEMPLATE)
#include "implementations/static_template.hpp"
#endif

int main() {
    inherited_explicit::run();
    inherited_ref_vtable::run();
    inherited_virtual::run();
    static_methods::run();
#if defined(TRP_TEST_STATIC_TEMPLATE)
    static_template::run();
#endif
}
