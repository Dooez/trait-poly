# Trait-Based Runtime Polymorphism in C++26

This repository is an experimental implementation of runtime polymorphism through
structural traits and type-erased trait handles.

[Godbolt Example](https://godbolt.org/z/xbx4M494G)  

Minimal example:
```cpp
#include "trp/shared_trait_ptr.hpp"

struct drawable {
    void draw() const;
};

struct circle {
    void draw() const;
};

static_assert(trp::any_trait<drawable>);
static_assert(trp::implements_trait<circle, drawable const>);

void draw_object(trp::shared_trait_ptr<drawable const> ptr){
    ptr->draw();
}

auto p = trp::make_shared_trait<drawable const, circle>(); 
draw_object(p);
```

## Trait Definitions

A TRP trait is a class type accepted by concept `trp::any_trait`. 
Trait must conform to the following limitations:
- all members and base classes are public;
- no virtual functions or virtual bases;
- no non-static data members;
- no user-provided, non-defaulted special member functions;
- no operators;
- no template methods;
- no rvalue-qualified and explicit-object-by-value methods;
- static data members are accepted only when recognized as `constexpr`. The
  current check is GCC-specific. Clang effectively rejects static data members;
- static function are only allowed as templates 
  of default implementations for trait methods;

## Trait Implementations
`trp::implements_trait<Impl, Trait>` is satisfied when for every method in `Trait`
a suitable implementation for `Impl` is found.
The implementation lookup is performed in the following order:
1. `trp::impl_spec_for<Impl, Trait>` specializations;
2. `trp::impl_spec_for<Impl, TraitBase>` for all bases of `Trait`; 
    > [!NOTE] 
    > The search is depth-first at the moment
3. Public direct members of `Impl` that satisfy all of:
    - matching identifier;
    - callable through cv-qualified reference to `Impl` with arguments specified by trait method;
    - invocation is noexcept if specified by trait;
    - return type exactly matches the type specified by trait;
3. 1-3 is repeated for public bases of `Impl`, width-first;
4. Trait defaults;

The lookup doesn't perform overload resolution.
Full overload resolution would be pretty complex to implement, and (as far as I know) 
not possible for function templates in C++26.
It is desired to implement basic resolution:
- build full viable overload set for each source in lookup list;
- use perfect match if present;
Return type matching is a possible customization point, but at the moment not implemented.

## Trait handle
The core handle is a type-erased `dyn_trait_ref<Trait>`.
This handle is a non-owning view over a trait object.
Method access uses dot notation `ref.foo();`. 
Since the dot notation is unavailable, interaction with `dyn_trait_ref` is provided via free functions:
- `trp::is_holding_type<Impl>(ref) -> bool` checks if implementation object is `Impl`. 
    Impl must match cv qualifications exactly.
- `trp::trait_cast<ExplicitSupertrait>(ref) -> dyn_trait_ref<ExplicitSupertrat>` 
    The vtable holds references to all explicit supertrait vtables, so this is always valid.
- `trp::trait_cast<Impl, AnotherTrait>(ref)` returns a trait reference assuming the object is `Impl`.
    If the underlying object is not of type `Impl`, calling any methods is potentially UB.
- `trp::is_valid_const_trait_cast<CVTrait>(ref)->bool` returns true if the underlying object implements
    cv-qualified `CVTrait`. `Trait` and `CVTrait` must be the same unqualified type.
- `trp::const_trait_cast<CVTrait>(ref)` returns a trait reference. Does not check for validity. 
    Calling any methods if the cast is invalid is potentially UB.

There are implementations of owning handles
- `trp::shared_trait_ptr` reference-counted trait handle
- `trp::unique_trait_ptr` new-allocated non-copiable trait handle
- `trp::alloca_unique_trait_ptr` allocator-aware non-copiable trait handle

The owning handles are very basic. Common API:
- `explicit operator bool()` checks whether the handle stores an object;
- `operator->` and `operator*` access the underlying `dyn_trait_ref`;
- `get() -> void*` returns the type-erased implementation pointer;
- `get<Impl>() -> Impl*` returns the implementation pointer when the runtime type is `Impl`, otherwise `nullptr`;
- `trp::is_holding_type<Impl>(ptr) -> bool` checks the implementation type;
- `trp::trait_cast<ExplicitSupertrait>(ptr)` moves/copies to an explicit supertrait handle;
- `trp::trait_cast<AnotherTrait, Impl>(ptr)` returns an empty owning handle unless the stored object is `Impl`.

Specific API:
- `trp::make_shared_trait<Trait, Impl>(...)` and `trp::allocate_shared_trait<Trait, Impl>(alloc, ...)`;
- `trp::make_unique_trait<Trait, Impl>(...)` and `trp::allocate_unique_trait<Trait, Impl>(alloc, ...)`;
- `trp::unique_trait_ptr<Trait>` can be moved into `trp::alloc_unique_trait_ptr<Trait>`.

Currently, `trp` does not have a clear way to define a custom owning handle.


## Current State
- [x] Definition of Traits
    - [x] Normal methods;
    - [x] cv-qualified methods;
    - [x] Noexcept qualification;
    - [x] Trait inheritance;
    - [x] Default implementations, inline and explicit;
    - [x] Explicit specialization of implementation methods;
    - [x] Concepts
        - `any_trait` - checks if a type is valid for trait definition;
        - `implements_trait` - checks if a type implements all methods of the trait;
        - `supertrait_of<S, T>` - checks if the method set of S is a subset of the method set of T;
        - `explicit_supertrait_of<S, T>` - `supertrait_of<S, T>` and S is in the inheritance chain of T, true for S == T;
        - `direct_supertrait_of<S, T>` - `supertrait_of<S, T>` and S is a direct base class of T, false for S == T;
- [x] Non-owning type-erased trait handle `dyn_trait_ref<T>`
    - [x] `trait_ref<cv_trait>` where `cv_trait` is cv-qualified
    - [x] cv-qualified `trait_ref<T>`
    - [x] Upcasting to `explicit_supertrait<S, T>` via `trait_cast<S>`
    - [x] Runtime type identification for implementations. via `bool trp::is_holding_type<Impl>(const trait_ref<T>&)`.
    - [x] Casting to other traits by providing implementation type via `trait_cast<T, Impl>`. Checked cast for owning handles, unchecked cast for trait_ref;
    - [ ] (?) Conversion to non-explicit supertraits for allocator-aware handles by constructing vtables at runtime.
- [x] Basic owning trait handles
    - [x] `shared_trait_ptr`, `unique_trait_ptr`, and `alloc_unique_trait_ptr`;
    - [x] `make_shared_trait`, `allocate_shared_trait`, `make_unique_trait`, and `allocate_unique_trait`.

## Implementation Details
C++26 reflection cannot generate types with methods; it can only generate aggregates with public data members.  
The methods of a polymorphic trait object are emulated by data members of type 
`method_invoker<...>` with the `[[no_unique_address]]` attribute and `operator()`.  
`method_holder<...>` is a standard-layout class with a first and only data member of type `method_invoker<...>`.  
`method_holder<...>` is defined via `std::meta::define_aggregate` to generate "methods" with the correct identifiers.  
`dyn_trait_ref_impl<MethodHolders...>` is the final required class. It is derived from all required holders and stores a vtable pointer and an object pointer.  
To access the vtable and object pointers from inside `operator()`, the following chain of casts is performed:  
1. The `this` pointer of `method_invoker::operator()(...)` is `reinterpret_cast` to `method_holder<...>*` (well-defined because `method_holder` is a standard-layout struct with a single member).
2. `method_holder<...>*` is `static_cast` to `dyn_trait_ref_impl<...>*` (well-defined due to inheritance).

Through the `trait_ref_impl*` pointer, the vtable and type-erased object pointers are acquired.

Because the access is done through a proxy, the cv-qualifications of `dyn_trait_ref<Trait>` must not be transient, 
and should be inferred from `Trait`.
To achieve it, `cvm_invoker` is used, that provides `operator()(auto const* vtable, void* obj, <method arguments>)` 
with correct cv qualifications for each overload. 
This way native C++ overload resolution is used for both argument type and cv resolution.

## Limitations
Compilation is relatively slow. Some effort was put to minimize repeated evalutation in implementation.  
Compilers may require `-fconstexpr-steps` with high number to successfully compile.  
`static constexpr` trait data members are supported by GCC only.  
`clangd` works but exhibits significant delays when handling trait objects.  
Some patterns in the `trp` implementation could be updated to more modern and cleaner versions with additional C++26 features as compiler support matures.  

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
- Configure with the gcc preset: `cmake --preset gcc`
- Build with the matching build preset: `cmake --build --preset <clang-p2996|gcc>`
