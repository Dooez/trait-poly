#include "trait_ptr/fancy_allocator.hpp"
#include "trait_ptr/shared.hpp"
#include "trait_ptr/unique.hpp"

int main() {
    unique::run();
    shared::run();
    fancy_allocator::run();
}
