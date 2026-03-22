# Trait-based Runtime Polymorphism in C++26
This is a proof of concept implementatin of runtime polymorphism through traits.
[Godbolt compiler explorer example](https://godbolt.org/z/oEe5EPjGh)  

Traits are defined with a struct declared with a number of non-template non-static methods and no data members.

The C++26 reflection cannot generate types with methods, only aggrefates with public data members.  
The methods of a polymorphic tait object are emulated by data members of type `method_invoker<...>` with attribute `[[no_unique_address]]`.
`method_invoker` is of an empty class type with `operator()` template derived from possible multiple `overload_invoker<...>` empty base classes, one for each overload signature.   
Each method invoker is a first member of standard-layout class `method_holder<...>`, defined via `std::meta::define_aggregate`. 
The final implementation class `trait_impl_manager<MethodHolders...>` is derived from all the required hodlers and also stores vtable pointer and object pointer.
To access vtable and object pointers from `operator()` the following chain of casts is performed:
1. `this` pointer of `overload_invoker::operator()(...)` is static_cast'ed to `method_invoker<...>*`. This is defined because of inheritance.
2. `method_invoker<...>*` is reinteret_cast'ed to `method_holder<...>*`. This is defined because `method_holder` is standard-layout struct with only one member.
3. `method_holder<...>*` is static_cast'ed to `trait_impl_manager<...>*`. This is defined because of inheritance.

Through the manager pointer vtable and type-erased object pointers are accessed, and the corresponding function pointer is invoked.
To perform these casts template arguments are used i.e. `overload_invoker<typename Manager,  MethodHolder, MethodInvoker, uZ Index, ...>`. 
`Index` argument is used to select the pointer in the vtable.
As far as I can know, this chain of casts does not include any undefined bahaviour.

When constructing vtable, the first implementation class method that invokable with arguments declared in trait and matching return type is used.
The method are wrapped to add preceding `void*` argument, that is casted to implementation method.
Vtable is constructed at comile time.

The compilation is relatively slow.

As a part of proof-of-concept the following is implemented:
- polymorphic trait objects with pointer semantics
- trait implementations can have templated methods that implement the required interface
- key concepts like `implements_trait<Impl, Trait>` and `any_trait<T>`

Not implemented, but possible and might be interesting:
- noexcept trait method qualification
- const trait method qualification
- `trait_reference<Trait>`
- `..._trait<const Trait>`
- some overload resolution in vtable construction
- definition of trait combinations
- dynamic casting between trait pointers
- an option for return type conversion for implementation methods
- user attributes to interact with special member functions e.g. `[[=tpr::use_copy_ctor]] clone()`
- small object optimisation
- non-type-erased reference wrapper to ensure restricted interface

At this moment the repository is for experimenting and sharing. The CMakeLists.txt is extremely basic and not made to be used as a library.

