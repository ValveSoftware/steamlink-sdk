# Callback<> and Bind()

## Introduction

The templated `Callback<>` class is a generalized function object. Together with
the `Bind()` function in base/bind.h, they provide a type-safe method for
performing partial application of functions.

Partial application (or "currying") is the process of binding a subset of a
function's arguments to produce another function that takes fewer arguments.
This can be used to pass around a unit of delayed execution, much like lexical
closures are used in other languages. For example, it is used in Chromium code
to schedule tasks on different MessageLoops.

A callback with no unbound input parameters (`Callback<void()>`) is called a
`Closure`. Note that this is NOT the same as what other languages refer to as a
closure -- it does not retain a reference to its enclosing environment.

### OnceCallback<> And RepeatingCallback<>

`OnceCallback<>` and `RepeatingCallback<>` are next gen callback classes, which
are under development.

`OnceCallback<>` is created by `BindOnce()`. This is a callback variant that is
a move-only type and can be run only once. This moves out bound parameters from
its internal storage to the bound function by default, so it's easier to use
with movable types. This should be the preferred callback type: since the
lifetime of the callback is clear, it's simpler to reason about when a callback
that is passed between threads is destroyed.

`RepeatingCallback<>` is created by `BindRepeating()`. This is a callback
variant that is copyable that can be run multiple times. It uses internal
ref-counting to make copies cheap. However, since ownership is shared, it is
harder to reason about when the callback and the bound state are destroyed,
especially when the callback is passed between threads.

The legacy `Callback<>` is currently aliased to `RepeatingCallback<>`. In new
code, prefer `OnceCallback<>` where possible, and use `RepeatingCallback<>`
otherwise. Once the migration is complete, the type alias will be removed and
`OnceCallback<>` will be renamed to `Callback<>` to emphasize that it should be
preferred.

`RepeatingCallback<>` is convertible to `OnceCallback<>` by the implicit
conversion.

### Memory Management And Passing

Pass `Callback` objects by value if ownership is transferred; otherwise, pass it
by const-reference.

```cpp
// |Foo| just refers to |cb| but doesn't store it nor consume it.
bool Foo(const OnceCallback<void(int)>& cb) {
  return cb.is_null();
}

// |Bar| takes the ownership of |cb| and stores |cb| into |g_cb|.
OnceCallback<void(int)> g_cb;
void Bar(OnceCallback<void(int)> cb) {
  g_cb = std::move(cb);
}

// |Baz| takes the ownership of |cb| and consumes |cb| by Run().
void Baz(OnceCallback<void(int)> cb) {
  std::move(cb).Run(42);
}

// |Qux| takes the ownership of |cb| and transfers ownership to PostTask(),
// which also takes the ownership of |cb|.
void Qux(OnceCallback<void(int)> cb) {
  PostTask(FROM_HERE, std::move(cb));
}
```

When you pass a `Callback` object to a function parameter, use `std::move()` if
you don't need to keep a reference to it, otherwise, pass the object directly.
You may see a compile error when the function requires the exclusive ownership,
and you didn't pass the callback by move.

## Quick reference for basic stuff

### Binding A Bare Function

```cpp
int Return5() { return 5; }
OnceCallback<int()> func_cb = BindOnce(&Return5);
LOG(INFO) << std::move(func_cb).Run();  // Prints 5.
```

```cpp
int Return5() { return 5; }
RepeatingCallback<int()> func_cb = BindRepeating(&Return5);
LOG(INFO) << func_cb.Run();  // Prints 5.
```

### Binding A Captureless Lambda

```cpp
Callback<int()> lambda_cb = Bind([] { return 4; });
LOG(INFO) << lambda_cb.Run();  // Print 4.

OnceCallback<int()> lambda_cb2 = BindOnce([] { return 3; });
LOG(INFO) << std::move(lambda_cb2).Run();  // Print 3.
```

### Binding A Class Method

The first argument to bind is the member function to call, the second is the
object on which to call it.

```cpp
class Ref : public RefCountedThreadSafe<Ref> {
 public:
  int Foo() { return 3; }
};
scoped_refptr<Ref> ref = new Ref();
Callback<void()> ref_cb = Bind(&Ref::Foo, ref);
LOG(INFO) << ref_cb.Run();  // Prints out 3.
```

By default the object must support RefCounted or you will get a compiler
error. If you're passing between threads, be sure it's RefCountedThreadSafe! See
"Advanced binding of member functions" below if you don't want to use reference
counting.

### Running A Callback

Callbacks can be run with their `Run` method, which has the same signature as
the template argument to the callback. Note that `OnceCallback::Run` consumes
the callback object and can only be invoked on a callback rvalue.

```cpp
void DoSomething(const Callback<void(int, std::string)>& callback) {
  callback.Run(5, "hello");
}

void DoSomethingOther(OnceCallback<void(int, std::string)> callback) {
  std::move(callback).Run(5, "hello");
}
```

RepeatingCallbacks can be run more than once (they don't get deleted or marked
when run). However, this precludes using Passed (see below).

```cpp
void DoSomething(const RepeatingCallback<double(double)>& callback) {
  double myresult = callback.Run(3.14159);
  myresult += callback.Run(2.71828);
}
```

### Passing Unbound Input Parameters

Unbound parameters are specified at the time a callback is `Run()`. They are
specified in the `Callback` template type:

```cpp
void MyFunc(int i, const std::string& str) {}
Callback<void(int, const std::string&)> cb = Bind(&MyFunc);
cb.Run(23, "hello, world");
```

### Passing Bound Input Parameters

Bound parameters are specified when you create the callback as arguments to
`Bind()`. They will be passed to the function and the `Run()`ner of the callback
doesn't see those values or even know that the function it's calling.

```cpp
void MyFunc(int i, const std::string& str) {}
Callback<void()> cb = Bind(&MyFunc, 23, "hello world");
cb.Run();
```

A callback with no unbound input parameters (`Callback<void()>`) is called a
`Closure`. So we could have also written:

```cpp
Closure cb = Bind(&MyFunc, 23, "hello world");
```

When calling member functions, bound parameters just go after the object
pointer.

```cpp
Closure cb = Bind(&MyClass::MyFunc, this, 23, "hello world");
```

### Partial Binding Of Parameters

You can specify some parameters when you create the callback, and specify the
rest when you execute the callback.

```cpp
void MyFunc(int i, const std::string& str) {}
Callback<void(const std::string&)> cb = Bind(&MyFunc, 23);
cb.Run("hello world");
```

When calling a function bound parameters are first, followed by unbound
parameters.

### Avoiding Copies with Callback Parameters

A parameter of `Bind()` is moved into its internal storage if it is passed as a
rvalue.

```cpp
std::vector<int> v = {1, 2, 3};
// |v| is moved into the internal storage without copy.
Bind(&Foo, std::move(v));
```

```cpp
std::vector<int> v = {1, 2, 3};
// The vector is moved into the internal storage without copy.
Bind(&Foo, std::vector<int>({1, 2, 3}));
```

A bound object is moved out to the target function if you use `Passed()` for
the parameter. If you use `BindOnce()`, the bound object is moved out even
without `Passed()`.

```cpp
void Foo(std::unique_ptr<int>) {}
std::unique_ptr<int> p(new int(42));

// |p| is moved into the internal storage of Bind(), and moved out to |Foo|.
BindOnce(&Foo, std::move(p));
BindRepeating(&Foo, Passed(&p));
```

## Quick reference for advanced binding

### Binding A Class Method With Weak Pointers

```cpp
Bind(&MyClass::Foo, GetWeakPtr());
```

The callback will not be run if the object has already been destroyed.
**DANGER**: weak pointers are not threadsafe, so don't use this when passing between
threads!

### Binding A Class Method With Manual Lifetime Management

```cpp
Bind(&MyClass::Foo, Unretained(this));
```

This disables all lifetime management on the object. You're responsible for
making sure the object is alive at the time of the call. You break it, you own
it!

### Binding A Class Method And Having The Callback Own The Class

```cpp
MyClass* myclass = new MyClass;
Bind(&MyClass::Foo, Owned(myclass));
```

The object will be deleted when the callback is destroyed, even if it's not run
(like if you post a task during shutdown). Potentially useful for "fire and
forget" cases.

Smart pointers (e.g. `std::unique_ptr<>`) are also supported as the receiver.

```cpp
std::unique_ptr<MyClass> myclass(new MyClass);
Bind(&MyClass::Foo, std::move(myclass));
```

### Ignoring Return Values

Sometimes you want to call a function that returns a value in a callback that
doesn't expect a return value.

```cpp
int DoSomething(int arg) { cout << arg << endl; }
Callback<void(int)> cb =
    Bind(IgnoreResult(&DoSomething));
```

## Quick reference for binding parameters to Bind()

Bound parameters are specified as arguments to `Bind()` and are passed to the
function. A callback with no parameters or no unbound parameters is called a
`Closure` (`Callback<void()>` and `Closure` are the same thing).

### Passing Parameters Owned By The Callback

```cpp
void Foo(int* arg) { cout << *arg << endl; }
int* pn = new int(1);
Closure foo_callback = Bind(&foo, Owned(pn));
```

The parameter will be deleted when the callback is destroyed, even if it's not
run (like if you post a task during shutdown).

### Passing Parameters As A unique_ptr

```cpp
void TakesOwnership(std::unique_ptr<Foo> arg) {}
std::unique_ptr<Foo> f(new Foo);
// f becomes null during the following call.
RepeatingClosure cb = BindRepeating(&TakesOwnership, Passed(&f));
```

Ownership of the parameter will be with the callback until the callback is run,
and then ownership is passed to the callback function. This means the callback
can only be run once. If the callback is never run, it will delete the object
when it's destroyed.

### Passing Parameters As A scoped_refptr

```cpp
void TakesOneRef(scoped_refptr<Foo> arg) {}
scoped_refptr<Foo> f(new Foo);
Closure cb = Bind(&TakesOneRef, f);
```

This should "just work." The closure will take a reference as long as it is
alive, and another reference will be taken for the called function.

```cpp
void DontTakeRef(Foo* arg) {}
scoped_refptr<Foo> f(new Foo);
Closure cb = Bind(&DontTakeRef, RetainedRef(f));
```

`RetainedRef` holds a reference to the object and passes a raw pointer to
the object when the Callback is run.

### Passing Parameters By Reference

Const references are *copied* unless `ConstRef` is used. Example:

```cpp
void foo(const int& arg) { printf("%d %p\n", arg, &arg); }
int n = 1;
Closure has_copy = Bind(&foo, n);
Closure has_ref = Bind(&foo, ConstRef(n));
n = 2;
foo(n);                        // Prints "2 0xaaaaaaaaaaaa"
has_copy.Run();                // Prints "1 0xbbbbbbbbbbbb"
has_ref.Run();                 // Prints "2 0xaaaaaaaaaaaa"
```

Normally parameters are copied in the closure.
**DANGER**: `ConstRef` stores a const reference instead, referencing the
original parameter. This means that you must ensure the object outlives the
callback!

## Implementation notes

### Where Is This Design From:

The design `Callback` and Bind is heavily influenced by C++'s `tr1::function` /
`tr1::bind`, and by the "Google Callback" system used inside Google.

### Customizing the behavior

There are several injection points that controls `Bind` behavior from outside of
its implementation.

```cpp
template <typename Receiver>
struct IsWeakReceiver {
  static constexpr bool value = false;
};

template <typename Obj>
struct UnwrapTraits {
  template <typename T>
  T&& Unwrap(T&& obj) {
    return std::forward<T>(obj);
  }
};
```

If `IsWeakReceiver<Receiver>::value` is true on a receiver of a method, `Bind`
checks if the receiver is evaluated to true and cancels the invocation if it's
evaluated to false. You can specialize `IsWeakReceiver` to make an external
smart pointer as a weak pointer.

`UnwrapTraits<BoundObject>::Unwrap()` is called for each bound arguments right
before `Callback` calls the target function. You can specialize this to define
an argument wrapper such as `Unretained`, `ConstRef`, `Owned`, `RetainedRef` and
`Passed`.

### How The Implementation Works:

There are three main components to the system:
  1) The `Callback<>` classes.
  2) The `Bind()` functions.
  3) The arguments wrappers (e.g., `Unretained()` and `ConstRef()`).

The Callback classes represent a generic function pointer. Internally, it stores
a refcounted piece of state that represents the target function and all its
bound parameters. The `Callback` constructor takes a `BindStateBase*`, which is
upcasted from a `BindState<>`. In the context of the constructor, the static
type of this `BindState<>` pointer uniquely identifies the function it is
representing, all its bound parameters, and a `Run()` method that is capable of
invoking the target.

`Bind()` creates the `BindState<>` that has the full static type, and erases the
target function type as well as the types of the bound parameters. It does this
by storing a pointer to the specific `Run()` function, and upcasting the state
of `BindState<>*` to a `BindStateBase*`. This is safe as long as this
`BindStateBase` pointer is only used with the stored `Run()` pointer.

To `BindState<>` objects are created inside the `Bind()` functions.
These functions, along with a set of internal templates, are responsible for

 - Unwrapping the function signature into return type, and parameters
 - Determining the number of parameters that are bound
 - Creating the BindState storing the bound parameters
 - Performing compile-time asserts to avoid error-prone behavior
 - Returning an `Callback<>` with an arity matching the number of unbound
   parameters and that knows the correct refcounting semantics for the
   target object if we are binding a method.

The `Bind` functions do the above using type-inference and variadic templates.

By default `Bind()` will store copies of all bound parameters, and attempt to
refcount a target object if the function being bound is a class method. These
copies are created even if the function takes parameters as const
references. (Binding to non-const references is forbidden, see bind.h.)

To change this behavior, we introduce a set of argument wrappers (e.g.,
`Unretained()`, and `ConstRef()`).  These are simple container templates that
are passed by value, and wrap a pointer to argument.  See the file-level comment
in base/bind_helpers.h for more info.

These types are passed to the `Unwrap()` functions to modify the behavior of
`Bind()`.  The `Unwrap()` functions change behavior by doing partial
specialization based on whether or not a parameter is a wrapper type.

`ConstRef()` is similar to `tr1::cref`.  `Unretained()` is specific to Chromium.

### Missing Functionality
 - Binding arrays to functions that take a non-const pointer.
   Example:
```cpp
void Foo(const char* ptr);
void Bar(char* ptr);
Bind(&Foo, "test");
Bind(&Bar, "test");  // This fails because ptr is not const.
```

If you are thinking of forward declaring `Callback` in your own header file,
please include "base/callback_forward.h" instead.
