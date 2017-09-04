# std::unique_ptr Transition Guide

Now `std::unique_ptr<T>` is available anywhere in Blink. This document goes through various use cases of `OwnPtr<T>`
and how they get converted to `std::unique_ptr<T>` so you can smoothly switch your mind.

If you have any uncertainties on using `std::unique_ptr<T>`, ask yutak@chromium.org (or other C++ gurus around you)
for help.

## Creation and Use

|||---|||
#### OwnPtr

```c++
void f()
{
    OwnPtr<T> owned = adoptPtr(new T(argument));
    owned->useThis();
    T& reference = *owned;
    T* pointer = owned.get();
    owned.clear();
    owned = adoptPtr(new T);
    T* leakedPointer = owned.leakPtr();
}
```

#### std::unique_ptr

```c++
void f()
{
    std::unique_ptr<T> owned(new T(argument));
    // Or: auto owned = wrapUnique(new T(argument)); †1
    owned->useThis();
    T& reference = *owned;
    T* pointer = owned.get();
    owned.reset();      // Or: owned = nullptr
    owned.reset(new T); // Or: owned = wrapUnique(new T)
    T* leakedPointer = owned.release();
}
```
|||---|||

†1 `wrapUnique()` is particularly useful when you pass a `unique_ptr` to somebody else:

```c++
std::unique_ptr<T> g()
{
    h(wrapUnique(new T(argument))); // Pass to a function.
    return wrapUnique(new T(argument)); // Return from a function.
}

```

You need to `#include "wtf/PtrUtil.h"` for `wrapUnique()`.

## Passing and Receiving

|||---|||
#### OwnPtr

```c++
void receive(PassOwnPtr<T> object)
{
    OwnPtr<T> localObject = object;
}

void handOver(PassOwnPtr<T> object)
{
    receive(object);
}

void give()
{
    OwnPtr<T> object = adoptPtr(new T);
    handOver(object.release());
    // |object| becomes null.
}

void giveDirectly()
{
    handOver(adoptPtr(new T));
}

PassOwnPtr<T> returning()
{
    OwnPtr<T> object = adoptPtr(new T);
    return object.release();
}

PassOwnPtr<T> returnDirectly()
{
    return adoptPtr(new T);
}
```

#### std::unique_ptr

```c++
void receive(std::unique_ptr<T> object)
{
    // It is not necessary to take the object locally. †1
}

void handOver(std::unique_ptr<T> object)
{
    receive(std::move(object)); // †2
}

void give()
{
    std::unique_ptr<T> object(new T);
    handOver(std::move(object)); // †2
    // |object| becomes null.
}

void giveDirectly()
{
    handOver(std::unique_ptr<T>(new T)); // †3
}

std::unique_ptr<T> returning()
{
    std::unique_ptr<T> object(new T);
    return object; // †4
}

std::unique_ptr<T> returnDirectly()
{
    return std::unique_ptr<T>(new T); // †3, 4
}
```

|||---|||

†1 Both `OwnPtr<T>` and `PassOwnPtr<T>` correspond to `std::unique_ptr<T>`, so a `std::unique_ptr<T>` in the
arugment and a `std::unique_ptr<T>` in the local variable are exactly the same.

†2 When you release the ownership of an lvalue, you need to surround it with `std::move()`. What's an lvalue? If
a value has a name and you can take its address, it's almost certainly an lvalue. In this example, `object` is
an lvalue.

†3 You don't have to do anything if you release the ownership of an rvalue. An rvalue is a value that is not
an lvalue, such as literals (`"foobar"`) or temporaries (`111 + 222`).

†4 `return` statements are kind of special in that they don't require `std::move()` on releasing ownership, even with
an lvalue. This is possible because `return` makes all the local variables go away by going back to the caller.

*** aside
*Note:* Well, yes, I know, the definitions of lvalues and rvalues here are not very precise. However, the above
understandings are sufficient in practice. If you want to know further, see
[the wiki about value categories at cppreference.com](http://en.cppreference.com/w/cpp/language/value_category).

See also [Dana's guide for rvalue references](https://sites.google.com/a/chromium.org/dev/rvalue-references)
if you want to learn about move semantics.
***
