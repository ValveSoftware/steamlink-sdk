// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptPromiseResolverWithContext_h
#define ScriptPromiseResolverWithContext_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ExecutionContext.h"
#include "platform/Timer.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <v8.h>

namespace WebCore {

// This class wraps ScriptPromiseResolver and provides the following
// functionalities in addition to ScriptPromiseResolver's.
//  - A ScriptPromiseResolverWithContext retains a ScriptState. A caller
//    can call resolve or reject from outside of a V8 context.
//  - This class is an ActiveDOMObject and keeps track of the associated
//    ExecutionContext state. When the ExecutionContext is suspended,
//    resolve or reject will be delayed. When it is stopped, resolve or reject
//    will be ignored.
class ScriptPromiseResolverWithContext : public ActiveDOMObject, public RefCounted<ScriptPromiseResolverWithContext> {
    WTF_MAKE_NONCOPYABLE(ScriptPromiseResolverWithContext);

public:
    static PassRefPtr<ScriptPromiseResolverWithContext> create(ScriptState* scriptState)
    {
        RefPtr<ScriptPromiseResolverWithContext> resolver = adoptRef(new ScriptPromiseResolverWithContext(scriptState));
        resolver->suspendIfNeeded();
        return resolver.release();
    }

    virtual ~ScriptPromiseResolverWithContext()
    {
        // This assertion fails if:
        //  - promise() is called at least once and
        //  - this resolver is destructed before it is resolved, rejected or
        //    the associated ExecutionContext is stopped.
        ASSERT(m_state == ResolvedOrRejected || !m_isPromiseCalled);
    }

    // Anything that can be passed to toV8Value can be passed to this function.
    template <typename T>
    void resolve(T value)
    {
        resolveOrReject(value, Resolving);
    }

    // Anything that can be passed to toV8Value can be passed to this function.
    template <typename T>
    void reject(T value)
    {
        resolveOrReject(value, Rejecting);
    }

    ScriptState* scriptState() { return m_scriptState.get(); }

    // Note that an empty ScriptPromise will be returned after resolve or
    // reject is called.
    ScriptPromise promise()
    {
#if ASSERT_ENABLED
        m_isPromiseCalled = true;
#endif
        return m_resolver ? m_resolver->promise() : ScriptPromise();
    }

    ScriptState* scriptState() const { return m_scriptState.get(); }

    // ActiveDOMObject implementation.
    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void stop() OVERRIDE;

    // Once this function is called this resolver stays alive while the
    // promise is pending and the associated ExecutionContext isn't stopped.
    void keepAliveWhilePending();

protected:
    // You need to call suspendIfNeeded after the construction because
    // this is an ActiveDOMObject.
    explicit ScriptPromiseResolverWithContext(ScriptState*);

private:
    enum ResolutionState {
        Pending,
        Resolving,
        Rejecting,
        ResolvedOrRejected,
    };
    enum LifetimeMode {
        Default,
        KeepAliveWhilePending,
    };

    template<typename T>
    v8::Handle<v8::Value> toV8Value(const T& value)
    {
        return ToV8Value<ScriptPromiseResolverWithContext, v8::Handle<v8::Object> >::toV8Value(value, m_scriptState->context()->Global(), m_scriptState->isolate());
    }

    template <typename T>
    void resolveOrReject(T value, ResolutionState newState)
    {
        if (m_state != Pending || !executionContext() || executionContext()->activeDOMObjectsAreStopped())
            return;
        ASSERT(newState == Resolving || newState == Rejecting);
        m_state = newState;
        // Retain this object until it is actually resolved or rejected.
        // |deref| will be called in |clear|.
        ref();

        ScriptState::Scope scope(m_scriptState.get());
        m_value.set(m_scriptState->isolate(), toV8Value(value));
        if (!executionContext()->activeDOMObjectsAreSuspended())
            resolveOrRejectImmediately();
    }

    void resolveOrRejectImmediately();
    void onTimerFired(Timer<ScriptPromiseResolverWithContext>*);
    void clear();

    ResolutionState m_state;
    const RefPtr<ScriptState> m_scriptState;
    LifetimeMode m_mode;
    Timer<ScriptPromiseResolverWithContext> m_timer;
    RefPtr<ScriptPromiseResolver> m_resolver;
    ScopedPersistent<v8::Value> m_value;
#if ASSERT_ENABLED
    // True if promise() is called.
    bool m_isPromiseCalled;
#endif
};

} // namespace WebCore

#endif // #ifndef ScriptPromiseResolverWithContext_h
