/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CallbackPromiseAdapter_h
#define CallbackPromiseAdapter_h

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "public/platform/WebCallbacks.h"
#include "wtf/PtrUtil.h"
#include "wtf/TypeTraits.h"
#include <memory>

namespace blink {

// CallbackPromiseAdapter is a WebCallbacks subclass and resolves / rejects the
// stored resolver when onSuccess / onError is called, respectively.
//
// Basically CallbackPromiseAdapter<S, T> is a subclass of
// WebCallbacks<S::WebType, T::WebType>. There are some exceptions:
//  - If S or T don't have WebType (e.g. S = bool), a default WebType holder
//    called trivial WebType holder is used. For example,
//    CallbackPromiseAdapter<bool, void> is a subclass of
//    WebCallbacks<bool, void>.
//  - If a WebType is std::unique_ptr<T>, its corresponding type parameter on
//    WebCallbacks is std::unique_ptr<T>, because WebCallbacks must be exposed to
//    Chromium.
//
// When onSuccess is called with a S::WebType value, the value is passed to
// S::take and the resolver is resolved with its return value. Ditto for
// onError.
//
// ScriptPromiseResolver::resolve / reject will not be called when the execution
// context is stopped.
//
// Example:
// class MyClass {
// public:
//     using WebType = std::unique_ptr<WebMyClass>;
//     static PassRefPtr<MyClass> take(ScriptPromiseResolver* resolver,
//         std::unique_ptr<WebMyClass> webInstance)
//     {
//         return MyClass::create(webInstance);
//     }
//     ...
// };
// class MyErrorClass {
// public:
//     using WebType = const WebMyErrorClass&;
//     static MyErrorClass take(ScriptPromiseResolver* resolver,
//         const WebErrorClass& webError)
//     {
//         return MyErrorClass(webError);
//     }
//     ...
// };
// std::unique_ptr<WebCallbacks<std::unique_ptr<WebMyClass>, const WebMyErrorClass&>>
//     callbacks = wrapUnique(new CallbackPromiseAdapter<MyClass, MyErrorClass>(
//     resolver));
// ...
//
// std::unique_ptr<WebCallbacks<bool, const WebMyErrorClass&>> callbacks2 =
//     wrapUnique(new CallbackPromiseAdapter<bool, MyErrorClass>(resolver));
// ...
//
//
// In order to implement the above exceptions, we have template classes below.
// OnSuccess and OnError provide onSuccess and onError implementation, and there
// are utility templates that provide
//  - std::unique_ptr - WebPassOwnPtr translation ([Web]PassType[Impl], adopt, pass),
//  - trivial WebType holder (TrivialWebTypeHolder).

namespace internal {

// This template is placed outside of CallbackPromiseAdapterInternal because
// explicit specialization is forbidden in a class scope.
template <typename T>
struct CallbackPromiseAdapterTrivialWebTypeHolder {
    using WebType = T;
    static T take(ScriptPromiseResolver*, const T& x) { return x; }
};
template <>
struct CallbackPromiseAdapterTrivialWebTypeHolder<void> {
    using WebType = void;
};

class CallbackPromiseAdapterInternal {
private:
    template <typename T> static T webTypeHolderMatcher(typename std::remove_reference<typename T::WebType>::type*);
    template <typename T> static CallbackPromiseAdapterTrivialWebTypeHolder<T> webTypeHolderMatcher(...);
    template <typename T> using WebTypeHolder = decltype(webTypeHolderMatcher<T>(nullptr));

    // The following templates should be gone when the repositories are merged
    // and we can use C++11 libraries.
    template <typename T>
    struct PassTypeImpl {
        using Type = T;
    };
    template <typename T>
    struct PassTypeImpl<std::unique_ptr<T>> {
        using Type = std::unique_ptr<T>;
    };
    template <typename T>
    struct WebPassTypeImpl {
        using Type = T;
    };
    template <typename T>
    struct WebPassTypeImpl<std::unique_ptr<T>> {
        using Type = std::unique_ptr<T>;
    };
    template <typename T> using PassType = typename PassTypeImpl<T>::Type;
    template <typename T> using WebPassType = typename WebPassTypeImpl<T>::Type;
    template <typename T> static T& adopt(T& x) { return x; }
    template <typename T>
    static std::unique_ptr<T> adopt(std::unique_ptr<T>& x) { return std::move(x); }
    template <typename T> static PassType<T> pass(T& x) { return x; }
    template <typename T> static std::unique_ptr<T> pass(std::unique_ptr<T>& x) { return std::move(x); }

    template <typename S, typename T>
    class Base : public WebCallbacks<WebPassType<typename S::WebType>, WebPassType<typename T::WebType>> {
    public:
        explicit Base(ScriptPromiseResolver* resolver) : m_resolver(resolver) {}
        ScriptPromiseResolver* resolver() { return m_resolver; }

    private:
        Persistent<ScriptPromiseResolver> m_resolver;
    };

    template <typename S, typename T>
    class OnSuccess : public Base<S, T> {
    public:
        explicit OnSuccess(ScriptPromiseResolver* resolver) : Base<S, T>(resolver) {}
        void onSuccess(WebPassType<typename S::WebType> r) override
        {
            typename S::WebType result(adopt(r));
            ScriptPromiseResolver* resolver = this->resolver();
            if (!resolver->getExecutionContext() || resolver->getExecutionContext()->activeDOMObjectsAreStopped())
                return;
            resolver->resolve(S::take(resolver, pass(result)));
        }
    };
    template <typename T>
    class OnSuccess<CallbackPromiseAdapterTrivialWebTypeHolder<void>, T> : public Base<CallbackPromiseAdapterTrivialWebTypeHolder<void>, T> {
    public:
        explicit OnSuccess(ScriptPromiseResolver* resolver) : Base<CallbackPromiseAdapterTrivialWebTypeHolder<void>, T>(resolver) {}
        void onSuccess() override
        {
            ScriptPromiseResolver* resolver = this->resolver();
            if (!resolver->getExecutionContext() || resolver->getExecutionContext()->activeDOMObjectsAreStopped())
                return;
            resolver->resolve();
        }
    };
    template <typename S, typename T>
    class OnError : public OnSuccess<S, T> {
    public:
        explicit OnError(ScriptPromiseResolver* resolver) : OnSuccess<S, T>(resolver) {}
        void onError(WebPassType<typename T::WebType> e) override
        {
            typename T::WebType result(adopt(e));
            ScriptPromiseResolver* resolver = this->resolver();
            if (!resolver->getExecutionContext() || resolver->getExecutionContext()->activeDOMObjectsAreStopped())
                return;
            resolver->reject(T::take(resolver, pass(result)));
        }
    };
    template <typename S>
    class OnError<S, CallbackPromiseAdapterTrivialWebTypeHolder<void>> : public OnSuccess<S, CallbackPromiseAdapterTrivialWebTypeHolder<void>> {
    public:
        explicit OnError(ScriptPromiseResolver* resolver) : OnSuccess<S, CallbackPromiseAdapterTrivialWebTypeHolder<void>>(resolver) {}
        void onError() override
        {
            ScriptPromiseResolver* resolver = this->resolver();
            if (!resolver->getExecutionContext() || resolver->getExecutionContext()->activeDOMObjectsAreStopped())
                return;
            resolver->reject();
        }
    };

public:
    template <typename S, typename T>
    class CallbackPromiseAdapter final : public OnError<WebTypeHolder<S>, WebTypeHolder<T>> {
        WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
    public:
        explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver) : OnError<WebTypeHolder<S>, WebTypeHolder<T>>(resolver) {}
    };
};

} // namespace internal

template <typename S, typename T>
using CallbackPromiseAdapter = internal::CallbackPromiseAdapterInternal::CallbackPromiseAdapter<S, T>;

} // namespace blink

#endif
