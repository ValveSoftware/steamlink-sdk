# Blink GC API reference

This is a through document for Oilpan API usage.
If you want to learn the API usage quickly, look at
[this tutorial](https://docs.google.com/presentation/d/1XPu03ymz8W295mCftEC9KshH9Icxfq81YwIJQzQrvxo/edit#slide=id.p).

[TOC]

## Header file

Unless otherwise noted, any of the primitives explained in this page requires the following `#include` statement:

```c++
#include "platform/heap/Handle.h"
```

## Base class templates

### GarbageCollected

A class that wants the lifetime management of its instances to be managed by Blink GC (Oilpan), it must inherit from
`GarbageCollected<YourClass>`.

```c++
class YourClass : public GarbageCollected<YourClass> {
    // ...
};
```

Instances of such classes are said to be *on Oilpan heap*, or *on heap* for short, while instances of other classes
are called *off heap*. In the rest of this document, the terms "on heap" or "on-heap objects" are used to mean the
objects on Oilpan heap instead of on normal (default) dynamic allocator's heap space.

You can create an instance of your class normally through `new`, while you may not free the object with `delete`,
as the Blink GC system is responsible for deallocating the object once it determines the object is unreachable.

You may not allocate an on-heap object on stack.

Your class may need to have a tracing method. See [Tracing](#Tracing) for details.

If your class needs finalization (i.e. some work needs to be done on destruction), use
[GarbageCollectedFinalized](#GarbageCollectedFinalized) instead.

`GarbageCollected<T>` or any class deriving from `GarbageCollected<T>`, directly or indirectly, must be the first
element in its base class list (called "leftmost derivation rule"). This rule is needed to assure each on-heap object
has its own canonical address.

```c++
class A : public GarbageCollected<A>, public P { // OK, GarbageCollected<A> is leftmost.
};

class B : public A, public Q { // OK, A is leftmost.
};

// class C : public R, public A { // BAD, A must be the first base class.
// };
```

If a non-leftmost base class needs to retain an on-heap object, that base class needs to inherit from
[GarbageCollectedMixin](#GarbageCollectedMixin). It's generally recommended to make *any* non-leftmost base class
inherit from `GarbageCollectedMixin`, because it's dangerous to save a pointer to a non-leftmost
non-`GarbageCollectedMixin` subclass of an on-heap object.

```c++
void someFunction(P*);

class A : public GarbageCollected<A>, public P {
public:
    void someMemberFunction()
    {
        someFunction(this); // DANGEROUS, a raw pointer to an on-heap object.
    }
};
```

### GarbageCollectedFinalized

If you want to make your class garbage-collected and the class needs finalization, your class needs to inherit from
`GarbageCollectedFinalized<YourClass>` instead of `GarbageCollected<YourClass>`.

A class is said to *need finalization* when it meets either of the following criteria:

*   It has non-empty destructor; or
*   It has a member that needs finalization.

```c++
class YourClass : public GarbageCollectedFinalized<YourClass> {
public:
    ~YourClass() { ... } // Non-empty destructor means finalization is needed.

private:
    RefPtr<Something> m_something; // RefPtr<> has non-empty destructor, so finalization is needed.
};
```

Note that finalization is done at an arbitrary time after the object becomes unreachable.

Any destructor executed within the finalization period must not touch any other on-heap object, because destructors
can be executed in any order. If there is a need of having such destructor, consider using
[EAGERLY_FINALIZE](#EAGERLY_FINALIZE).

Because `GarbageCollectedFinalized<T>` is a special case of `GarbageCollected<T>`, all the restrictions that apply
to `GarbageCollected<T>` classes also apply to `GarbageCollectedFinalized<T>`.

### GarbageCollectedMixin

A non-leftmost base class of a garbage-collected class may derive from `GarbageCollectedMixin`. If a direct child
class of `GarbageCollected<T>` or `GarbageCollectedFinalized<T>` has a non-leftmost base class deriving from
`GarbageCollectedMixin`, the garbage-collected class must declare the `USING_GARBAGE_COLLECTED_MIXIN(ClassName)` macro
in its class declaration.

A class deriving from `GarbageCollectedMixin` can be treated similarly as garbage-collected classes. Specifically, it
can have `Member<T>`s and `WeakMember<T>`s, and a tracing method. A pointer to such a class must be retained in the
same smart pointer wrappers as a pointer to a garbage-collected class, such as `Member<T>` or `Persistent<T>`.
The tracing method of a garbage-collected class, if any, must contain a delegating call for each mixin base class.

```c++
class P : public GarbageCollectedMixin {
public:
    DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_q); } // OK, needs to trace m_q.
private:
    Member<Q> m_q; // OK, allowed to have Member<T>.
};

class A : public GarbageCollected<A>, public P {
    USING_GARBAGE_COLLECTED_MIXIN(A);
public:
    DEFINE_INLINE_VIRTUAL_TRACE() { ...; P::trace(visitor); } // Delegating call for P is needed.
    ...
};
```

Internally, `GarbageCollectedMixin` defines pure virtual functions, and `USING_GARBAGE_COLLECTED_MIXIN(ClassName)`
implements these virtual functions. Therefore, you cannot instantiate a class that is a descendant of
`GarbageCollectedMixin` but not a descendant of `GarbageCollected<T>`. Two or more base classes inheritng from
`GarbageCollectedMixin` can be resolved with a single `USING_GARBAGE_COLLECTED_MIXIN(ClassName)` declaration.

```c++
class P : public GarbageCollectedMixin { };
class Q : public GarbageCollectedMixin { };
class R : public Q { };

class A : public GarbageCollected<A>, public P, public R {
    USING_GARBAGE_COLLECTED_MIXIN(A); // OK, resolving pure virtual functions of P and R.
};

class B : public GarbageCollected<B>, public P {
    USING_GARBAGE_COLLECTED_MIXIN(B); // OK, different garbage-collected classes may inherit from the same mixin (P).
};

void someFunction()
{
    new A; // OK, A can be instantiated.
    // new R; // BAD, R has pure virtual functions.
}
```

## Class properties

### USING_GARBAGE_COLLECTED_MIXIN

`USING_GARBAGE_COLLECTED_MIXIN(ClassName)` is a macro that must be declared in a garbage-collected class, if any of
its base classes is a descendant of `GarbageCollectedMixin`.

See [GarbageCollectedMixin](#GarbageCollectedMixin) for the use of `GarbageCollectedMixin` and this macro.

### USING_PRE_FINALIZER

`USING_PRE_FINALIZER(ClassName, functionName)` in a class declaration declares the class has a *pre-finalizer* of name
`functionName`.

A pre-finalizer is a user-defined member function of a garbage-collected class that is called when the object is going
to be swept but before the garbage collector actually sweeps any objects. Therefore, it is allowed for a pre-finalizer
to touch any other on-heap objects, while a destructor is not. It is useful for doing some cleanups that cannot be done
with a destructor.

A pre-finalizer must have the following function signature: `void preFinalizer()`. You can change the function name.

A pre-finalizer must be registered in the constructor by using the following statement:
"`ThreadState::current()->registerPreFinalizer(this);`".

```c++
class YourClass : public GarbageCollectedFinalized<YourClass> {
    USING_PRE_FINALIZER(YourClass, dispose);
public:
    YourClass()
    {
        ThreadState::current()->registerPreFinalizer(this);
    }
    void dispose()
    {
        m_other->dispose(); // OK; you can touch other on-heap objects in a pre-finalizer.
    }
    ~YourClass()
    {
        // m_other->dispose(); // BAD.
    }

private:
    Member<OtherClass> m_other;
};
```

Pre-finalizers have some implications on the garbage collector's performance: the garbage-collector needs to iterate
all registered pre-finalizers at every GC. Therefore, a pre-finalizer should be avoided unless it is really necessary.
Especially, avoid defining a pre-finalizer in a class that can be allocated a lot.

### EAGERLY_FINALIZE

### DISALLOW_NEW_EXCEPT_PLACEMENT_NEW

### STACK_ALLOCATED

## Handles

Class templates in this section are smart pointers, each carrying a pointer to an on-heap object (think of `RefPtr<T>`
for `RefCounted<T>`). Collectively, they are called *handles*.

On-heap objects must be retained by any of these, depending on the situation.

### Raw pointers

On-stack references to on-heap objects must be raw pointers.

```c++
void someFunction()
{
    SomeGarbageCollectedClass* object = new SomeGarbageCollectedClass; // OK, retained by a pointer.
    ...
}
// OK to leave the object behind. The Blink GC system will free it up when it becomes unused.
```

### Member, WeakMember

In a garbage-collected class, on-heap objects must be retained by `Member<T>` or `WeakMember<T>`, depending on
the desired semantics.

`Member<T>` represents a *strong* reference to an object of type `T`, which means that the referred object is kept
alive as long as the owner class instance is alive. Unlike `RefPtr<T>`, it is okay to form a reference cycle with
members (in on-heap objects) and raw pointers (on stack).

`WeakMember<T>` is a *weak* reference to an object of type `T`. Unlike `Member<T>`, `WeakMember<T>` does not keep
the pointed object alive. The pointer in a `WeakMember<T>` can become `nullptr` when the object gets garbage-collected.
It may take some time for the pointer in a `WeakMember<T>` to become `nullptr` after the object actually goes unused,
because this rewrite is only done within Blink GC's garbage collection period.

```c++
class SomeGarbageCollectedClass : public GarbageCollected<GarbageCollectedSomething> {
    ...
private:
    Member<AnotherGarbageCollectedClass> m_another; // OK, retained by Member<T>.
    WeakMember<AnotherGarbageCollectedClass> m_anotherWeak; // OK, weak reference.
};
```

The use of `WeakMember<T>` incurs some overhead in garbage collector's performance. Use it sparingly. Usually, weak
members are not necessary at all, because reference cycles with members are allowed.

More specifically, `WeakMember<T>` should be used only if the owner of a weak member can outlive the pointed object.
Otherwise, `Member<T>` should be used.

You need to trace every `Member<T>` and `WeakMember<T>` in your class. See [Tracing](#Tracing).

### Persistent, WeakPersistent, CrossThreadPersistent, CrossThreadWeakPersistent

In a non-garbage-collected class, on-heap objects must be retained by `Persistent<T>`, `WeakPersistent<T>`,
`CrossThreadPersistent<T>`, or `CrossThreadWeakPersistent<T>`, depending on the situations and the desired semantics.

`Persistent<T>` is the most basic handle in the persistent family, which makes the referred object alive
unconditionally, as long as the persistent handle is alive.

`WeakPersistent<T>` does not make the referred object alive, and becomes `nullptr` when the object gets
garbage-collected, just like `WeakMember<T>`.

`CrossThreadPersistent<T>` and `CrossThreadWeakPersistent<T>` are cross-thread variants of `Persistent<T>` and
`WeakPersistent<T>`, respectively, which can point to an object in a different thread.

```c++
class NonGarbageCollectedClass {
    ...
private:
    Persistent<SomeGarbageCollectedClass> m_something; // OK, the object will be alive while this persistent is alive.
};
```

*** note
**Warning:** `Persistent<T>` and `CrossThreadPersistent<T>` are vulnerable to reference cycles. If a reference cycle
is formed with `Persistent`s, `Member`s, `RefPtr`s and `OwnPtr`s, all the objects in the cycle **will leak**, since
nobody in the cycle can be aware of whether they are ever referred from anyone.

When you are about to add a new persistent, be careful not to create a reference cycle. If a cycle is inevitable, make
sure the cycle is eventually cut by someone outside the cycle.
***

Persistents have small overhead in itself, because they need to maintain the list of all persistents. Therefore, it's
not a good idea to create or keep a lot of persistents at once.

Weak variants have overhead just like `WeakMember<T>`. Use them sparingly.

The need of cross-thread persistents may indicate a poor design in multi-thread object ownership. Think twice if they
are really necessary.

## Tracing

A garbage-collected class may need to have *a tracing method*, which lists up all the on-heap objects it has. The
tracing method is called when the garbage collector needs to determine (1) all the on-heap objects referred from a
live object, and (2) all the weak handles that may be filled with `nullptr` later. These are done in the "marking"
phase of the mark-and-sweep GC.

The basic form of tracing is illustrated below:

```c++
// In a header file:
class SomeGarbageCollectedClass : public GarbageCollected<SomeGarbageCollectedClass> {
public:
    DECLARE_TRACE();

private:
    Member<AnotherGarbageCollectedClass> m_another;
};

// In an implementation file:
DEFINE_TRACE(SomeGarbageCollectedClass)
{
    visitor->trace(m_another);
}
```

Specifically, if your class needs a tracing method, you need to:

*   Declare a tracing method in your class declaration, using the `DECLARE_TRACE()` macro; and
*   Define a tracing method in your implementation file, using the `DEFINE_TRACE(ClassName)` macro.

The function implementation must contain:

*   For each on-heap object `m_object` in your class, a tracing call: "```visitor->trace(m_object);```".
*   If your class has one or more weak references (`WeakMember<T>`), you have the option of
    registering a *weak callback* for the object. See details below for how.
*   For each base class of your class `BaseClass` that is a descendant of `GarbageCollected<T>` or
    `GarbageCollectedMixin`, a delegation call to base class: "```BaseClass::trace(visitor);```"

It is recommended that the delegation call, if any, is put at the end of a tracing method.

If the class does not contain any on-heap object, the tracing method is not needed.

If you want to define your tracing method inline or need to have your tracing method polymorphic, you can use the
following variants of the tracing macros:

*   "```DECLARE_VIRTUAL_TRACE();```" in a class declaration makes the method ```virtual```. Use
    "```DEFINE_TRACE(ClassName) { ... }```" in the implementation file to define.
*   "```DEFINE_INLINE_TRACE() { ... }```" in a class declaration lets you define the method inline. If you use this,
    you may not write "```DEFINE_TRACE(ClassName) { ... }```" in your implementation file.
*   "```DEFINE_INLINE_VIRTUAL_TRACE() { ... }```" in a class declaration does both of the above.

The following example shows more involved usage:

```c++
class A : public GarbageCollected<A> {
public:
    DEFINE_INLINE_VIRTUAL_TRACE() { } // Nothing to trace here. Just to declare a virtual method.
};

class B : public A {
    // Nothing to trace here; exempted from having a tracing method.
};

class C : public B {
public:
    DECLARE_VIRTUAL_TRACE();

private:
    Member<X> m_x;
    WeakMember<Y> m_y;
    HeapVector<Member<Z>> m_z;
};

DEFINE_TRACE(C)
{
    visitor->trace(m_x);
    visitor->trace(m_y); // Weak member needs to be traced.
    visitor->trace(m_z); // Heap collection does, too.
    B::trace(visitor); // Delegate to the parent. In this case it's empty, but this is required.
}
```

Given that the class `C` above contained a `WeakMember<Y>` field, you could alternatively
register a *weak callback* in the trace method, and have it be invoked after the marking
phase:

```c++

void C::clearWeakMembers(Visitor* visitor)
{
    if (ThreadHeap::isHeapObjectAlive(m_y))
        return;

    // |m_y| is not referred to by anyone else, clear the weak
    // reference along with updating state / clearing any other
    // resources at the same time. None of those operations are
    // allowed to perform heap allocations:
    m_y->detach();

    // Note: if the weak callback merely clears the weak reference,
    // it is much simpler to just |trace| the field rather than
    // install a custom weak callback.
    m_y = nullptr;
}

DEFINE_TRACE(C)
{
    visitor->template registerWeakMembers<C, &C::clearWeakMembers>(this);
    visitor->trace(m_x);
    visitor->trace(m_z); // Heap collection does, too.
    B::trace(visitor); // Delegate to the parent. In this case it's empty, but this is required.
}
```

Please notice that if the object (of type `C`) is also not reachable, its `trace` method
will not be invoked and any follow-on weak processing will not be done. Hence, if the
object must always perform some operation when the weak reference is cleared, that
needs to (also) happen during finalization.

Weak callbacks have so far seen little use in Blink, but a mechanism that's available.

## Heap collections
