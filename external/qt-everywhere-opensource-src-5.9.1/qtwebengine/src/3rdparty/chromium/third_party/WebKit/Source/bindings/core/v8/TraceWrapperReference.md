# Wrapper Tracing Reference

This document describes wrapper tracing and how its API is supposed to be used.

[TOC]

## Quickstart guide

Wrapper tracing is used to represent reachability across V8 and Blink. The
following checklist highlights the modifications needed to make a class
participate in wrapper tracing.

1. Make sure that objects participating in tracing either inherit from
``ScriptWrappable`` (if they can reference wrappers) or ``TraceWrapperBase``
(transitively holding wrappers alive).
2. Use ``TraceWrapperMember<T>`` to annotate fields that need to be followed to
find other wrappers that this object should keep alive.
3. Use ``TraceWrapperV8Reference<T>`` to annotate references to V8 that this
object should keep alive.
4. Declare a method to trace other wrappers using
``DECLARE_VIRTUAL_TRACE_WRAPPERS()``.
5. Define the method using ``DEFINE_TRACE_WRAPPERS(ClassName)``.
6. Trace all fields that received a wrapper tracing type in (1) and (2) using
``visitor->traceWrapers(<m_field>)`` in the body of ``DEFINE_TRACE_WRAPPERS``.

The following example illustrates these steps:

```c++
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "bindings/core/v8/TraceWrapperV8Reference.h"

class SomeDOMObject : public ScriptWrappable {          // (1)
 public:
  DECLARE_VIRTUAL_TRACE_WRAPPERS();                     // (4)

 private:
  TraceWrapperMember<OtherWrappable> m_otherWrappable;  // (2)
  Member<NonWrappable> m_nonWrappable;
  TraceWrapperV8Reference<v8::Value> m_v8object;        // (3)
  // ...
};

DEFINE_VIRTUAL_TRACE_WRAPPERS(SomeDOMObject) {          // (5)
  visitor->traceWrappers(m_otherWrappable);             // (6)
  visitor->traceWrappers(m_v8object);                   // (6)
}
```

For more in-depth information and how to deal with corner cases continue on reading.

## Background

Blink and V8 need to cooperate to collect JavaScript *wrappers*. Each V8
*wrapper* object (*W*) in JavaScript is assigned a C++ *DOM object* (*D*) in
Blink. A single C++ *DOM object* can hold onto one or many *wrapper* objects.
During a garbage collection initiated from JavaScript, a *wrapper* then keeps
the  C++ *DOM object* and all its transitive dependencies, including other
*wrappers*, alive, effectively tracing paths like
*W<sub>x</sub> -> D<sub>1</sub>  -> ⋯ -> D<sub>n</sub> -> W<sub>y</sub>*.

Previously, this relationship was expressed using so-called object groups,
effectively assigning all C++ *DOM objects* in a given DOM tree the same group.
The group would only die if all objects belonging to the same group die. A brief
introduction on the limitations on this approach can be found in
[this slide deck][object-grouping-slides].

Wrapper tracing solves this problem by determining liveness based on
reachability by tracing through the C++ *DOM objects*. The rest of this document
describes how the API is used to keep JavaScript wrapper objects alive.

Note that *wrappables* have to be *on-heap objects* and thus all
[Oilpan-related rules][oilpan-docs] apply.

[object-grouping-slides]: https://docs.google.com/presentation/d/1I6leiRm0ysSTqy7QWh33Gfp7_y4ngygyM2tDAqdF0fI/
[oilpan-docs]: https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/platform/heap/BlinkGCAPIReference.md

## Basic usage

The annotations that are required can be found in the following header files.
Pick the header file depending on what types are needed.

```c++
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperBase.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "bindings/core/v8/TraceWrapperV8Reference.h"
```

The following example will guide through the modifications that are needed to
adjust a given class ``SomeDOMObject`` to participate in wrapper tracing.

```c++
class SomeDOMObject : public ScriptWrappable {
  // ...
 private:
  Member<OtherWrappable /* extending ScriptWrappable */> m_otherWrappable;
  Member<NonWrappable> m_nonWrappable;
};
```

In this scenario ``SomeDOMObject`` is the object that is wrapped by an object on
the JavaScript side. The next step is to identify the paths that lead to other
wrappables. In this case, only ``m_otherWrappable`` needs to be traced to find
other *wrappers* in V8.

As wrapper tracing only traces a subset of members residing on the Oilpan heap,
it requires its own tracing method. The macros are as follows:

* ``DECLARE_VIRTUAL_TRACE_WRAPPERS();``: Use in the class declaration to declare
the needed wrapper tracing method.
* ``DEFINE_TRACE_WRAPPERS(ClassName)``: Use to define the implementation of
wrapper tracing.

Many more convenience wrappers, like inline definitions, can be found in
``platform/heap/WrapperVisitor.h``.

```c++
class SomeDOMObject : public ScriptWrappable {
 public:
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  Member<OtherWrappable> m_otherWrappable;
  Member<NonWrappable> m_nonWrappable;
};

DEFINE_VIRTUAL_TRACE_WRAPPERS(SomeDOMObject) {
  visitor->traceWrappers(m_otherWrappable);
}
```


Blink and V8 implement *incremental* wrapper tracing, which means that marking
can be interleaved with JavaSCript or even DOM operations. This poses a
challenge, because already marked objects will not be considered again if they
are reached through some other path.

For example, consider an object ``A`` that has already been marked and a write
to a field ``A.x`` setting ``x`` to an unmarked object ``Y``.  Since ``A.x`` is
the only reference keeping ``Y``, and ``A`` has already been marked, the garbage
collector will not find ``Y`` and reclaim it.

To overcome this problem we require all writes of interesting objects, i.e.,
writes to traced fields, to go through a write barrier.  Repeat for simpler
readers: The write barrier will check for the problem case above and make sure
``Y`` will be marked. In order to automatically issue a write barrier
``m_otherWrappable`` needs ``TraceWrapperMember`` type.

```c++
class SomeDOMObject : public ScriptWrappable {
 public:
  SomeDOMObject() : m_otherWrappable(this, nullptr) {}
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  TraceWrapperMember<OtherWrappable> m_otherWrappable;
  Member<NonWrappable> m_nonWrappable;
};

DEFINE_VIRTUAL_TRACE_WRAPPERS(SomeDOMObject) {
  visitor->traceWrappers(m_otherWrappable);
}
```

``TraceWrapperMember`` makes sure that any write to ``m_otherWrappable`` will
consider doing a write barrier. Using the proper type, the write barrier is
correct by construction, i.e., it will never be missed.

## Heap collections

The proper type usage for collections, e.g. ``HeapVector`` looks like the
following.

```c++
class SomeDOMObject : public ScriptWrappable {
 public:
  // ...
  void AppendNewValue(OtherWrappable* newValue);
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  HeapVector<TraceWrapperMember<OtherWrappable>> m_otherWrappables;
};

DEFINE_VIRTUAL_TRACE_WRAPPERS(SomeDOMObject) {
  for (auto other : m_otherWrappables)
    visitor->traceWrappers(other);
}
```

Note that this is different to Oilpan which can just trace the whole collection.
Whenever an element is added through ``append()`` the value needs to be
constructed using ``TraceWrapperMember``, e.g.

```c++
void SomeDOMObject::AppendNewValue(OtherWrappable* newValue) {
  m_otherWrappables.append(TraceWrapperMember(this, newValue));
}
```

The compiler will throw an error for each ommitted ``TraceWrapperMember``
construction.

### Corner-case: Pre-sized containers

Containers know how to construct an empty ``TraceWrapperMember`` slot. This
allows simple creation of containers at the cost of loosing the compile-time
check for assigning a raw value.

```c++
class SomeDOMObject : public ScriptWrappable {
 public:
  SomeDOMObject() {
    m_otherWrappables.resize(5);
  }

  void writeAt(int i, OtherWrappable* other) {
    m_otherWrappables[i] = other;
  }

  DECLARE_VIRTUAL_TRACE_WRAPPERS();
 private:
  HeapVector<TraceWrapperMember<OtherWrappable>> m_otherWrappables;
};

DEFINE_VIRTUAL_TRACE_WRAPPERS(SomeDOMObject) {
  for (auto other : m_otherWrappables)
    visitor->traceWrappers(other);
}
```

In this example, the compiler will not warn you on
``m_otherWrappables[i] = other``, but an assertion will throw at runtime as long
as there exists a test covering that branch.

The correct assignment looks like

```c++
m_otherWrappables[i] = TraceWrapperMember<OtherWrappable>(this, other);
```

Note that the assertion that triggers when the annotation is not present does
not require wrapper tracing to be enabled.

## Tracing through non-``ScriptWrappable`` types

Sometimes it is necessary to trace through types that do not inherit from
``ScriptWrappable``. For example, consider the object graph
``A -> B -> C`` where both ``A`` and ``C`` are ``ScriptWrappable``s that
need to be traced.

In this case, the same rules as with ``ScriptWrappables`` apply, except for the
difference that these classes need to inherit from ``TraceWrapperBase``.

### Memory-footprint critical uses

In the case we cannot afford inheriting from ``TraceWrapperBase``, which will
add a vtable pointer for tracing wrappers, an entry in a macro
``WRAPPER_VISITOR_SPECIAL_CASES``
is required in
``platform/heap/WrapperVisitor.h``.

## Explicit write barriers

Sometimes it is necessary to stick with the regular types and issue the write
barriers explicitly. For example, if memory footprint is really important and
it's not possible to use ``TraceWrapperMember`` which adds another pointer
field. In this case, tracing needs to be adjusted to tell the system that all
barriers will be done manually.

```c++
class ManualWrappable : public ScriptWrappable {
 public:
  void setNew(OtherWrappable* newValue) {
    m_otherWrappable = newValue;
    SriptWrappableVisitor::writeBarrier(this, m_otherWrappable);
  }

  DECLARE_VIRTUAL_TRACE_WRAPPERS();
 private:
  Member<OtherWrappable>> m_otherWrappable;
};

DEFINE_VIRTUAL_TRACE_WRAPPERS(ManualWrappable) {
  for (auto other : m_otherWrappables)
    visitor->traceWrappersWithManualWriteBarrier(other);
}
```