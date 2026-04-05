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

## Current state and limitations
Let `Impl` be the implementation class.
When constructing vtable, the chosen function is the first member function or template of `Impl` that is invokable with arguments declared in trait and has a matching return type.
The methods are wrapped to add preceding `void*` argument, that is casted to implementation method.
Vtable is constructed at compile time.

The compilation is relatively slow.

As a part of proof-of-concept the following is implemented:
- polymorphic trait objects with pointer semantics
- trait implementations can have templated methods that implement the required interface
- key concepts like `implements_trait<Impl, Trait>` and `any_trait<T>`

## Exploration
Not implemented, but probably possible and might be interesting:
- support for functor data members in implementations, besides functions and templates
- noexcept trait method qualification
- const trait method qualification
- `trait_reference<Trait>`
- `..._trait<const Trait>`
- some overload resolution in vtable construction
- definition of trait combinations and corresponging concept checking (e.g. smallest common supertrait or a greatest common subtrait)
- dynamic upcasting to supertraits
- an option for return type conversion for implementation methods
- small object optimisation
- non-type-erased reference wrapper to ensure restricted interface (i.e. interfaces)

At this moment the repository is for experimenting and sharing. The CMakeLists.txt is extremely basic and not made to be used as a library.

