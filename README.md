# Trait-Based Runtime Polymorphism in C++26
This repository provides a basic implementation of runtime polymorphism through traits.  
[Godbolt Example](https://godbolt.org/z/qxGTjYv53)  
Traits are defined with a struct declared with non-template, non-static methods and no data members,
using a `consteval` function `define_trait<Trait>()`.

## Implementation Details
C++26 reflection cannot generate types with methods; it can only generate aggregates with public data members.  
The methods of a polymorphic trait object are emulated by data members of type `method_invoker<...>` with the `[[no_unique_address]]` attribute and `operator()`.  
`method_invoker<...>` is derived from potentially multiple `overload_invoker<...>` base classes, one for each overload signature.  
`method_invoker<...>` uses `overload_invoker::operator()`.  
`method_holder<...>` is a standard-layout class with a first and only data member of type `method_invoker<...>`.  
`method_holder<...>` is defined via `std::meta::define_aggregate` to generate "methods" with the correct identifiers.  
`trait_ref_impl<MethodHolders...>` is the final required class. It is derived from all required holders and stores a vtable pointer and an object pointer.  

To access the vtable and object pointers from inside `operator()`, the following chain of casts is performed:  
1. The `this` pointer of `overload_invoker::operator()(...)` is `static_cast` to `method_invoker<...>*` (well-defined due to inheritance).
2. `method_invoker<...>*` is `reinterpret_cast` to `method_holder<...>*` (well-defined because `method_holder` is a standard-layout struct with a single member).
3. `method_holder<...>*` is `static_cast` to `trait_ref_impl<...>*` (well-defined due to inheritance).

Through the `trait_ref_impl*` pointer, the vtable and type-erased object pointers are accessed, and the corresponding function pointer is invoked.  
The final types are passed as template arguments to enable these casts.  
A non-type template parameter `Index` selects the appropriate function pointer from the vtable, e.g., `overload_invoker<Manager, MethodHolder, MethodInvoker, uZ Index, ...>`.

As reasoned, this chain of casts avoids undefined behavior. Please open an issue otherwise.

## Current State
- [x] Definition of Traits
    - [x] Normal methods
    - [x] cv-qualified methods
    - [x] Noexcept qualification
    - [x] Trait inheritance
    - [x] Default implementations
    - [x] Concepts
        - `any_trait` - checks if a type is valid for trait definition
        - `implements_trait` - checks if a type implements all methods of the trait
        - `supertrait_of<S, T>` - checks if the method set of S is a subset of the method set of T
        - `explicit_supertrait_of<S, T>` - `supertrait_of<S, T>` and S is in the inheritance chain of T, true for S == T
        - `direct_supertrait_of<S, T>` - `supertrait_of<S, T>` and S is a direct base class of T, false for S == T
- [x] Non-owning type-erased trait handle `trait_ref<T>`
    - [x] `trait_ref<cv_trait>` where `cv_trait` is cv-qualified
    - [x] cv-qualified `trait_ref<T>`
    - [x] Upcasting to `explicit_supertrait<S, T>` via `trait_cast<S>`
    - [x] Runtime type identification for implementations. via `bool trp::is_holding_type<Impl>(const trait_ref<T>&)`.
    - [ ] (?) Downcasting after checking type identification.
    - [ ] (?) Conversion to non-explicit supertraits for allocator-aware handles by constructing vtables at runtime.

## Limitations
Compilation is relatively slow. Some effort was put to minimize repeated evalutation in implementation.  
Compilers may require `-fconstexpr-steps` with high number to successfully compile.  
`static constexpr` trait data members are supported by GCC only.  
`clangd` works but exhibits significant delays when handling trait objects.  
Some patterns in the `trp` implementation could be updated to more modern and cleaner versions with additional C++26 features as compiler support matures.  
`define_trait<Trait>()` must be called in global namespace (not sure if this is the gcc-specific beahvior or standard requires).  

Given the early stage of reflection compiler and tooling development, these issues may improve over time. 
However, current tooling challenges raise concerns about possible production usability.

## Exploration
Not implemented, but potentially feasible and interesting:
- Partial overload resolution of implementation methods during vtable construction
- Definition of trait combinations (e.g. greatest common supertrait or common subtrait wo/ inheritence and explicit definition)
- Option for return type conversion in implementation methods
- Small object optimization
- (?) Non-type-erased reference wrapper to enforce restricted interfaces

At this moment, the repository is for experimenting and sharing.

## Build examples
- Configure with the p2996 preset: `cmake --preset clang-p2996 -DTRP_P2996_INSTALL_PATH=/path/to/clang-p2996/install`
- Configure with the gcc preset: `cmake --preset gcc-latest -DTRP_GCC_INSTALL_PATH=/path/to/gcc/install`
- Build with the matching build preset: `cmake --build --preset <clang-p2996|gcc-latest>`
