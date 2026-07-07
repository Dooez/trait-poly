# Trait-Based Runtime Polymorphism in C++26

This repository is an experimental implementation of runtime polymorphism based
on structural traits and type-erased trait handles.

[Godbolt example](https://godbolt.org/z/K89r8v117)

## Minimal Example

```cpp
#include "trp/shared_trait_ptr.hpp"

struct drawable {
    void draw() const;
};

struct circle {
    void draw() const {};
};

static_assert(trp::any_trait<drawable>);
static_assert(trp::implements_trait<circle, drawable const>);

void draw_object(trp::shared_trait_ptr<drawable const> ptr) {
    ptr->draw();
}

int main() {
    auto p = trp::make_shared_trait<drawable const, circle>();
    draw_object(p);
}
```

## Trait Definitions

A TRP trait is a class type accepted by the `trp::any_trait` concept. A trait
must satisfy these restrictions:

- all members and base classes are public;
- no virtual functions or virtual bases;
- no non-static data members;
- no non-default special member functions;
- no operators;
- no non-static template methods;
- no rvalue-qualified methods or explicit-object-by-value methods;
- static data members are accepted only when recognized as `constexpr`. The
  current check is GCC-specific. Clang effectively rejects static data members;
- static functions are allowed only when they are templates used as default
  implementations for trait methods.

## Trait Implementations

`trp::implements_trait<Impl, Trait>` is satisfied when every method in `Trait`
has a suitable implementation for `Impl`.

Implementation lookup uses this order:

1. `trp::impl_spec_for<std::remove_cv_t<Impl>, std::remove_cv_t<Trait>>` specialization;

   > [!NOTE]
   > `trp::impl_spec_for<I, T>` requires `T` to be cv-unqualified.

2. `trp::impl_spec_for<std::remove_cv_t<Impl>, TraitBase>` for each base of `Trait`;

   > [!NOTE]
   > This search is depth-first.

3. Direct public members with matching identifier:
    1. First direct public members of `Impl` that do not satisfy all of are discarded:
        - the member is callable through a cv-qualified reference to `Impl` with the
          arguments specified by the trait method;
        - the invocation is `noexcept` if the trait method is `noexcept`;
        - the return type matches type specified by the trait method:  
          - matches exactly if trait or method are annotated with <not_implemented>, default behavior;
          - convertible to if trait or method are annotated with <not_implemented>.  
    3. trait method signature with potentially adjusted return type is checked.
       return type is adjusted if the method being checked returns a type diffreent 
       from initial trait signature;
    4. if cv-promotion is allowed cv qualified signatures are checked
       e.g. if return adjasted trait method signature is `int foo(int);` 
       and a member does not match it, then `int foo(int) const` etc. are checked;
    5. if parameter conversion is allowed an overload set is built and resolved 
       using normal C++ overload resolution.

4. If there are no direct public members with matching identifier,
   steps 1-3 are repeated for public bases of `Impl`, breadth-first.
   If there are multiple fitting candidates, the result is discarded.
5. Explicit trait defaults `trp::default_impl_spec<std::remove_cv_t<Trait>>`.
6. Inline trait defaults i.e. static members with matching signature:
   - method         `       auto foo(int) const -> int`
   - inline default `static auto foo(const auto&, int) -> int`

Overload resolution limitations:
- Function templates cannot be inspected in C++ 26. 
  There's no way to acquire instantiation from a call.
  The only reliable way to check function template is to check conversion to pointer to a member.
  Currently member function templates will only be selected if they match the signature exactly in the 3.2 or 3.3
  or if they are the only candidate and the parameter conversion is allowed.
  If there is more than one candidate in 3.5 and one of them is a template the overload resolution cannot be done.
- Reflections of *using-declaration* is not present in C++26. 
  This means that if there are multiple bases providing `foo` member,
  and the derived class uses *using-declaration* to disambiguate,
  the implementation overload resolution cannot be done.

## Trait Handles

### Variant
The non-type erased handle is `trp::trait_variant<Trait, Alternatives...>`.
It is owning wrapper with value semantics.

To prevent clashing with method object the following free functions are provided:

- `trp::index(Var const& var) -> tag_t` returns index of the current alternative
- `trp::valueless_by_exception(Var const& var) -> bool` returns false if variant holds a value
- `trp::emplace<I>(Var& var, Args&&... args)` constructs an alternative I in place
- `trp::emplace<T>(Var& var, Args&&... args)` constructs an alternative of type T in place
- `trp::get<I>(Variant&& var) -> decltype(auto)` returns a reference to alternative I
- `trp::get<T>(Variant&& var) -> decltype(auto)` returns a reference to alternative of type T
- `auto holds_alternative<T>(Var const& var)-> bool` returns true if current alternative is of type T
- `trp::get_if<I>(Variant* var) -> decltype(auto)` returns a pointer to alternative I, nullptr if not active
- `trp::get_if<T>(Variant* var) -> decltype(auto)` returns a pointer to alternative of type T, nullptr if not active

### Type-erased handles
The core the type-erased handle is `trp::dyn_trait_ref<Trait>`. It is a non-owning
view over a trait object.

Method access uses dot notation: `ref.foo();`. Because dot notation is reserved
for trait method access, operations on `dyn_trait_ref` itself are provided as
free functions:

- `trp::is_holding_type<Impl>(ref) -> bool` checks whether the implementation
  object is `Impl`. `Impl` must match cv-qualifiers exactly.
- `trp::trait_cast<ExplicitSupertrait>(ref) -> dyn_trait_ref<ExplicitSupertrait>`
  casts to an explicit supertrait. Any trait vtable includes all direct
  supertrait vtables, so this conversion is always valid.
- `trp::trait_cast<AnotherTrait, Impl>(ref)` returns a trait reference assuming
  the object is `Impl`. If the underlying object is not of type `Impl`, calling
  any method is potentially undefined behavior.
- `trp::is_valid_const_trait_cast<CVTrait>(ref) -> bool` returns true when the
  underlying object implements cv-qualified `CVTrait`. `Trait` and `CVTrait`
  must have the same unqualified type.
- `trp::const_trait_cast<CVTrait>(ref)` returns a trait reference without
  checking validity. Calling any method after an invalid cast is potentially
  undefined behavior.

The repository also includes these owning handles:

- `trp::shared_trait_ptr`, a reference-counted trait handle;
- `trp::unique_trait_ptr`, a new-allocated non-copyable trait handle;
- `trp::alloc_unique_trait_ptr`, an allocator-aware non-copyable trait handle.

The owning handles are currently basic. They share this API:

- `explicit operator bool()` checks whether the handle stores an object;
- `operator->` and `operator*` access the underlying `dyn_trait_ref`;
- `get() -> void*` returns the type-erased implementation pointer;
- `get<Impl>() -> Impl*` returns the implementation pointer when the runtime
  type is `Impl`, otherwise `nullptr`;
- `trp::is_holding_type<Impl>(ptr) -> bool` checks the implementation type;
- `trp::trait_cast<ExplicitSupertrait>(ptr)` moves or copies to an explicit
  supertrait handle;
- `trp::trait_cast<AnotherTrait, Impl>(ptr)` returns an empty owning handle
  unless the stored object is `Impl`.

Specific owning-handle APIs:

- `trp::make_shared_trait<Trait, Impl>(...)` and
  `trp::allocate_shared_trait<Trait, Impl>(alloc, ...)`;
- `trp::make_unique_trait<Trait, Impl>(...)` and
  `trp::allocate_unique_trait<Trait, Impl>(alloc, ...)`;
- `trp::unique_trait_ptr<Trait>` can be moved into
  `trp::alloc_unique_trait_ptr<Trait>`.

Currently, `trp` does not have a clear way to define a custom owning handle.

## Current State

- [x] Definition of traits
  - [x] Normal methods
  - [x] cv-qualified methods
  - [x] `noexcept` qualification
  - [x] Trait inheritance
  - [x] Default implementations, inline and explicit
  - [x] Explicit specialization of implementation methods
  - [x] Concepts:
    - `any_trait`: checks whether a type is valid as a trait definition;
    - `implements_trait`: checks whether a type implements all methods of the
      trait;
    - `supertrait_of<S, T>`: checks whether the method set of `S` is a subset
      of the method set of `T`;
    - `explicit_supertrait_of<S, T>`: `supertrait_of<S, T>` and `S` is in the
      inheritance chain of `T`; true for `S == T`;
    - `direct_supertrait_of<S, T>`: `supertrait_of<S, T>` and `S` is a direct
      base class of `T`; false for `S == T`.
- [x] `trait_variant<T, Alternatives...>`
- [x] Non-owning type-erased trait handle `dyn_trait_ref<T>`
  - [x] `dyn_trait_ref<cv_trait>` where `cv_trait` is cv-qualified
  - [x] Upcasting to `explicit_supertrait_of<S, T>` via `trait_cast<S>`
  - [x] Runtime type identification for implementations via
        `bool trp::is_holding_type<Impl>(const dyn_trait_ref<T>&)`
  - [x] Casting to other traits by providing the implementation type via
        `trait_cast<T, Impl>`. This is a checked cast for owning handles and an
        unchecked cast for `dyn_trait_ref`.
  - [ ] (?) Conversion to non-explicit supertraits for allocator-aware handles
        by constructing vtables at runtime
- [x] Basic owning trait handles
  - [x] `shared_trait_ptr`, `unique_trait_ptr`, and `alloc_unique_trait_ptr`
  - [x] `make_shared_trait`, `allocate_shared_trait`, `make_unique_trait`, and
        `allocate_unique_trait`

## Implementation Details

C++26 reflection cannot generate types with methods. It can only generate
aggregates with public data members.

The methods of a polymorphic trait object are emulated by data members of type
`method_invoker<...>` with the `[[no_unique_address]]` attribute and `operator()`.  
`/* method-holder */` type is a standard-layout class with a first and only data member of type `method_invoker<...>`.  
`/* method-holder */` is defined via `std::meta::define_aggregate` to generate "methods" with the correct identifiers.  
`dyn_trait_ref_impl<...>` is derived from all required holders and stores a vtable pointer and a type-erased object pointer.  
`/* dyn-ref-wrapper */` is derived from `dyn_trait_ref_impl<...>`.

To access the vtable and object pointers from inside `operator()`, the following
chain of casts is performed:

1. The `this` pointer of `method_invoker<...>::operator()(...)` is
   `reinterpret_cast` to a `/* method-holder */` pointer. This is well-defined
   because `/* method-holder */` is a standard-layout struct with a single
   member.
2. The `/* method-holder */` pointer is `static_cast` to a
   `/* dyn-ref-wrapper */` pointer. This is well-defined because of inheritance.

The vtable and type-erased object pointers are then acquired through the
`/* dyn-ref-wrapper */` pointer.

Because access happens through a proxy, the cv-qualifiers of
`dyn_trait_ref<Trait>` must not be transient and should be inferred from
`Trait`. To achieve this, `cvm_invoker<...>` provides
`operator()(auto const* vtable, void* obj, <method arguments>)` with the correct
cv-qualifiers for each overload. A cv-qualified `cvm_invoker<...>` with
qualifiers equal to the `Trait` qualifiers is constructed and invoked.

This uses native C++ overload resolution for both argument types and cv
resolution.

Variant uses similar technique, except it is cv-transient, and doesn't use 

## Limitations

Compilation is relatively slow. Some effort was made to minimize repeated
evaluation in the implementation.

Some patterns in the `trp` implementation could be updated to more modern and
cleaner versions with additional C++26 features as compiler support matures.

Given the early stage of reflection compiler and tooling development, these
issues may improve over time. However, current tooling challenges raise concerns
about possible production usability.

## Exploration

Not implemented, but potentially feasible and interesting:

- Variant with type-erased extension. 
  This can be viewed as speculatively devirtualized dynamic polymorphism.
- Definition of trait combinations, such as a greatest common supertrait or a
  common subtrait without inheritance and explicit definition
- Small object optimization
- (?) Non-type-erased reference wrapper to enforce restricted interfaces

At this moment, the repository is for experimenting and sharing.

## Build Examples

- Configure with the p2996 preset:
  `TRP_P2996_INSTALL_PATH=/path/to/clang-p2996/install cmake --preset clang-p2996`
- Configure with the GCC preset: `cmake --preset gcc`
- Build with the matching build preset:
  `cmake --build --preset <clang-p2996|gcc>`
