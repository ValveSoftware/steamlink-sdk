/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_Functional_h
#define WTF_Functional_h

#include "base/bind.h"
#include "base/threading/thread_checker.h"
#include "base/tuple.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/TypeTraits.h"
#include "wtf/WeakPtr.h"
#include <tuple>
#include <utility>

namespace blink {
template <typename T> class Member;
template <typename T> class WeakMember;
}

namespace base {

template <typename T>
struct IsWeakReceiver<WTF::WeakPtr<T>> : std::true_type {};

}

namespace WTF {

// Functional.h provides a very simple way to bind a function pointer and arguments together into a function object
// that can be stored, copied and invoked, similar to how boost::bind and std::bind in C++11.

// Thread Safety:
//
// WTF::bind() and WTF::Closure should be used for same-thread closures
// only, i.e. the closures must be created, executed and destructed on
// the same thread.
// Use crossThreadBind() and CrossThreadClosure if the function/task is called
// or destructed on a (potentially) different thread from the current thread.

// WTF::bind() and move semantics
// ==============================
//
// For unbound parameters (arguments supplied later on the bound functor directly), there are two ways to pass movable
// arguments:
//
//     1) Pass by rvalue reference.
//
//            void yourFunction(Argument&& argument) { ... }
//            std::unique_ptr<Function<void(Argument&&)>> functor = bind<Argument&&>(yourFunction);
//
//     2) Pass by value.
//
//            void yourFunction(Argument argument) { ... }
//            std::unique_ptr<Function<void(Argument)>> functor = bind<Argument>(yourFunction);
//
// Note that with the latter there will be *two* move constructions happening, because there needs to be at least one
// intermediary function call taking an argument of type "Argument" (i.e. passed by value). The former case does not
// require any move constructions inbetween.
//
// For bound parameters (arguments supplied on the creation of a functor), you can move your argument into the internal
// storage of the functor by supplying an rvalue to that argument (this is done in wrap() of ParamStorageTraits).
// However, to make the functor be able to get called multiple times, the stored object does not get moved out
// automatically when the underlying function is actually invoked. If you want to make an argument "auto-passed",
// you can do so by wrapping your bound argument with passed() function, as shown below:
//
//     void yourFunction(Argument argument)
//     {
//         // |argument| is passed from the internal storage of functor.
//         ...
//     }
//
//     ...
//     std::unique_ptr<Function<void()>> functor = bind(yourFunction, passed(Argument()));
//     ...
//     (*functor)();
//
// The underlying function must receive the argument wrapped by passed() by rvalue reference or by value.
//
// Obviously, if you create a functor this way, you shouldn't call the functor twice or more; after the second call,
// the passed argument may be invalid.

enum FunctionThreadAffinity {
    CrossThreadAffinity,
    SameThreadAffinity
};

template <typename T>
class PassedWrapper final {
public:
    explicit PassedWrapper(T&& scoper) : m_scoper(std::move(scoper)) { }
    PassedWrapper(PassedWrapper&& other) : m_scoper(std::move(other.m_scoper)) { }
    T moveOut() const { return std::move(m_scoper); }

private:
    mutable T m_scoper;
};

template <typename T>
PassedWrapper<T> passed(T&& value)
{
    static_assert(!std::is_reference<T>::value,
        "You must pass an rvalue to passed() so it can be moved. Add std::move() if necessary.");
    static_assert(!std::is_const<T>::value, "|value| must not be const so it can be moved.");
    return PassedWrapper<T>(std::move(value));
}

template <typename T, FunctionThreadAffinity threadAffinity>
class UnretainedWrapper final {
public:
    explicit UnretainedWrapper(T* ptr) : m_ptr(ptr) { }
    T* value() const { return m_ptr; }

private:
    T* m_ptr;
};

template <typename T>
UnretainedWrapper<T, SameThreadAffinity> unretained(T* value)
{
    static_assert(!WTF::IsGarbageCollectedType<T>::value, "unretained() + GCed type is forbidden");
    return UnretainedWrapper<T, SameThreadAffinity>(value);
}

template <typename T>
UnretainedWrapper<T, CrossThreadAffinity> crossThreadUnretained(T* value)
{
    static_assert(!WTF::IsGarbageCollectedType<T>::value, "crossThreadUnretained() + GCed type is forbidden");
    return UnretainedWrapper<T, CrossThreadAffinity>(value);
}

template <typename T>
struct ParamStorageTraits {
    typedef T StorageType;

    static_assert(!std::is_pointer<T>::value, "Raw pointers are not allowed to bind into WTF::Function. Wrap it with either wrapPersistent, wrapWeakPersistent, wrapCrossThreadPersistent, wrapCrossThreadWeakPersistent, RefPtr or unretained.");
    static_assert(!IsSubclassOfTemplate<T, blink::Member>::value && !IsSubclassOfTemplate<T, blink::WeakMember>::value, "Member and WeakMember are not allowed to bind into WTF::Function. Wrap it with either wrapPersistent, wrapWeakPersistent, wrapCrossThreadPersistent or wrapCrossThreadWeakPersistent.");
};

template <typename T>
struct ParamStorageTraits<PassRefPtr<T>> {
    typedef RefPtr<T> StorageType;
};

template <typename T>
struct ParamStorageTraits<RefPtr<T>> {
    typedef RefPtr<T> StorageType;
};

template <typename T>
T* Unwrap(const RefPtr<T>& wrapped)
{
    return wrapped.get();
}

template <typename> class RetainPtr;

template <typename T>
struct ParamStorageTraits<RetainPtr<T>> {
    typedef RetainPtr<T> StorageType;
};

template <typename T>
struct ParamStorageTraits<PassedWrapper<T>> {
    typedef PassedWrapper<T> StorageType;
};

template <typename T>
T Unwrap(const PassedWrapper<T>& wrapped)
{
    return wrapped.moveOut();
}

template <typename T, FunctionThreadAffinity threadAffinity>
struct ParamStorageTraits<UnretainedWrapper<T, threadAffinity>> {
    typedef UnretainedWrapper<T, threadAffinity> StorageType;
};

template <typename T, FunctionThreadAffinity threadAffinity>
T* Unwrap(const UnretainedWrapper<T, threadAffinity>& wrapped)
{
    return wrapped.value();
}

template<typename Signature, FunctionThreadAffinity threadAffinity = SameThreadAffinity>
class Function;

template<typename R, typename... Args, FunctionThreadAffinity threadAffinity>
class Function<R(Args...), threadAffinity> {
    USING_FAST_MALLOC(Function);
    WTF_MAKE_NONCOPYABLE(Function);
public:
    Function(base::Callback<R(Args...)> callback)
        : m_callback(std::move(callback)) { }

    ~Function()
    {
        DCHECK(m_threadChecker.CalledOnValidThread());
    }

    R operator()(Args... args)
    {
        DCHECK(m_threadChecker.CalledOnValidThread());
        return m_callback.Run(std::forward<Args>(args)...);
    }

    explicit operator base::Callback<R(Args...)>()
    {
        return m_callback;
    }

private:
    using MaybeThreadChecker = typename std::conditional<
        threadAffinity == SameThreadAffinity,
        base::ThreadChecker,
        base::ThreadCheckerDoNothing>::type;
    MaybeThreadChecker m_threadChecker;
    base::Callback<R(Args...)> m_callback;
};

template <FunctionThreadAffinity threadAffinity, typename FunctionType, typename... BoundParameters>
std::unique_ptr<Function<base::MakeUnboundRunType<FunctionType, BoundParameters...>, threadAffinity>> bindInternal(FunctionType function, BoundParameters&&... boundParameters)
{
    // Bound parameters' types are wrapped with std::tuple so we can pass two template parameter packs (bound
    // parameters and unbound) to PartBoundFunctionImpl. Note that a tuple of this type isn't actually created;
    // std::tuple<> is just for carrying the bound parameters' types. Any other class template taking a type parameter
    // pack can be used instead of std::tuple. std::tuple is used just because it's most convenient for this purpose.
    using UnboundRunType = base::MakeUnboundRunType<FunctionType, BoundParameters...>;
    return wrapUnique(new Function<UnboundRunType, threadAffinity>(base::Bind(function, typename ParamStorageTraits<typename std::decay<BoundParameters>::type>::StorageType(std::forward<BoundParameters>(boundParameters))...)));
}

template <typename FunctionType, typename... BoundParameters>
std::unique_ptr<Function<base::MakeUnboundRunType<FunctionType, BoundParameters...>, SameThreadAffinity>> bind(FunctionType function, BoundParameters&&... boundParameters)
{
    return bindInternal<SameThreadAffinity>(function, std::forward<BoundParameters>(boundParameters)...);
}

typedef Function<void(), SameThreadAffinity> Closure;
typedef Function<void(), CrossThreadAffinity> CrossThreadClosure;

} // namespace WTF

using WTF::passed;
using WTF::unretained;
using WTF::crossThreadUnretained;

using WTF::Function;
using WTF::CrossThreadClosure;

#endif // WTF_Functional_h
