# Trait-based Runtime Polymorphism in C++26
This is a basic implementatin of runtime polymorphism through traits.
Traits are defined with a struct declared with a number of non-template non-static methods and no data members 
and a `consteval` function `define_trait<trait>()`.

## Core techinque
The C++26 reflection cannot generate types with methods, only aggregates with public data members.  
The methods of a polymorphic tait object are emulated by data members with of type `method_invoker<...>` with attribute `[[no_unique_address]]`.  
`method_invoker<...>` is derived from potentially multiple `overload_invoker<...>` base classes, one for each overload signature.  
`method_invoker<...>` uses `overload_invoker::operator()`.  
`method_holder<...>` is a standard-layout class with first and only data member of type `method_invoker<...>`.  
`method_holder<...>` is defined via `std::meta::define_aggregate` to generate "methods" with the correct names.  
`trait_impl_manager<MethodHolders...>` is the final required class, is derived from all the required holders and also stores vtable pointer and object pointer.  

To access vtable and object pointers from inside `operator()` the following chain of casts is performed:  
1. `this` pointer of `overload_invoker::operator()(...)` is static_cast'ed to `method_invoker<...>*`. This is defined because of inheritance.
2. `method_invoker<...>*` is reinteret_cast'ed to `method_holder<...>*`. This is defined because `method_holder` is standard-layout struct with only one member.
3. `method_holder<...>*` is static_cast'ed to `trait_impl_manager<...>*`. This is defined because of inheritance.

Through the manager pointer vtable and type-erased object pointers are accessed, and the corresponding function pointer is invoked.  
To perform these casts the final types are passed as template arguments.  
`Index` non-type template argument is used to select the pointer in the vtable.  
i.e. `overload_invoker<typename Manager, MethodHolder, MethodInvoker, uZ Index, ...>`.  

As far as I can reason, this chain of casts does not include any undefined bahaviour.

## Current state
- [x] Definition of Triats 
    - [x] Normal methods
    - [x] cv-qualified methods
    - [x] Noexcept qualification
    - [x] Trait inheritence
    - [x] conepts 
        - `any_trait` - checks if the type is a valid for trait definition
        - `implements_trait` - check if type implements all method of a trait
        - `supertrait_of<S, T>` - checks if all methods of S are methods of T
        - `explicit_supertrait_of<S, T>` - `supertrait_of<S, T>` and part of inheritence chain, true for S == T
        - `direct_supertrait_of<S, T>` - `supertait_of<S, T>` and S is a direcct base class of T, false for S == T
- [x] Type erased trait handle non-owning `trait_ref<T>`
    - [x] `trait_ref<cv_trait>` where `cv_trait` is cv-qualified
    - [x] cv-qualified `trait_ref<T>`
    - [x] Upcasting to `explicit_supertrait<S, T>` via `trait_cast<S>`
    - [ ] Implementation method overload resolution. Currently first matched method is used.
    - [ ] (?) Runtime implementation type identification. Compile time constructed identifiers stored in 
    the vtable to check e.g. via `bool trp::is_underlying_type<Impl>(const trait_ref<T>&)`.

- [x] Basic owning handles
    - [x] `unique_trait_ptr` - lifetime management using `new`|`delete` 
    - [x] `alloca_unique_trait_ptr` - lifetime management with type-erased allocator support
    - [x] `shared_trait_ptr` - lifetime management with type-erased allocator support and basic reference counting
    - [x] Upcasting to `explicit_supertrait<S, T>` version via `trait_cast<S>`
    - [ ] (?) Conversion to `supertrait_of` for allocator-aware handles by constructing vtables at runtime

## Limitations
gcc is not tested yet, but planned.
The compilation is relatively slow.
`Clangd` works, but has a significant delay when dealing with triat handles.
Some `trp` implementation details could be improved with more C++26 features when compiler supports them.

Since this is an early stage of compiler and tooling development for reflection these might be improved.
At the moment I have doubdts about usability in production because of tooling issues.

## Exploration
Not implemented, but probably possible and might be interesting:
- some overload resolution in vtable construction
- definition of trait combinations and corresponging concept checking (e.g. smallest common supertrait or a greatest common subtrait)
- dynamic upcasting to supertraits
- an option for return type conversion for implementation methods
- small object optimisation
- (?) non-type-erased reference wrapper to ensure restricted interface (i.e. interfaces)

At this moment the repository is for experimenting and sharing.  
The CMakeLists.txt is extremely basic and not made to be used as a library.

