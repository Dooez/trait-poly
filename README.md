# Trait-based Runtime Polymorphism in C++26
This is a proof of concept implementatin of runtime polymorphism through traits.
[Godbolt compiler explorer example](https://godbolt.org/z/Y1oWqz7a8)  

Traits are defined with a struct declared with a number of non-template non-static methods and no data members.

The C++26 reflection cannot generate types with methods, only public data members.  
The methods of a polymorphic tait object are emulated by data members with `no_unique_address` of an empty class type with `operator()` template,
derived from multiple empty base classes, one for each trait method overload signature.   
`operator()` finds the first base that can be invoked with passed arguments.   
The polymorphic trait object is derived from manager struct that holds a type-erased pointer to the beginning of a "vtable" and a type-erased pointer to an object.  
The vtable is filled with pointers to a wrapper functions that convert type-erased object pointer to the real type and call corresponding functions.  
`operator()` of methods call the wrapper function through the vtable pointer and an index.   
To get the vtable from inside the method's `operator()` all the methods are assumed to have 0 offset from the beginning of polymorphic object (this is checked via static_assert).  
The `this` pointer is casted to the pointer to the manager struct, and thus type erased object and vtable ptr are acquired. 

As a part of proof-of-concept the following is implemented:
- concept for checking of trait implementation
- construction of shared ownership polymorphic trait object with specified methods
- trait implementations can have templated methods that implement the required interface

There are some important limitations:
- It's probably UB? I'm not sure, but the casting is very hacky. 
Even though the offsets are statically checked, i cannot provide a formal explanation or proof. 
If it is UB, probably nothing can be done to "fix" it.
- The overload resolution is almost non-existent. This can be improved, however, a full match for a normal language overload resolution at first glance 
seems unlikely or very labor intensive, considering all possible value categories and conversions.   
- Compilation is rather slow.  

Not implemented, but possible and might be interesting:
- noexcept trait method qualification
- const trait method qualification
- `unique_trait<Trait>`
- `reference_trait<Trait>`
- `..._trait<const Trait>`
- small object optimisation
- constructing traits from pointers/references and pointer-like objects
- non-type-erased reference wrapper to ensure restricted interface

At this moment the repository is for experimenting and sharing. The CMakeLists.txt is extremely basic and not made to be used as a library.
