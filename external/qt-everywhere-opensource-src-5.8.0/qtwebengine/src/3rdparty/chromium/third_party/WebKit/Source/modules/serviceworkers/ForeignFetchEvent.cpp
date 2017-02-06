// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ForeignFetchEvent.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "wtf/RefPtr.h"

namespace blink {
ForeignFetchEvent* ForeignFetchEvent::create()
{
    return new ForeignFetchEvent();
}

ForeignFetchEvent* ForeignFetchEvent::create(ScriptState* scriptState, const AtomicString& type, const ForeignFetchEventInit& initializer)
{
    return new ForeignFetchEvent(scriptState, type, initializer, nullptr, nullptr);
}

ForeignFetchEvent* ForeignFetchEvent::create(ScriptState* scriptState, const AtomicString& type, const ForeignFetchEventInit& initializer, ForeignFetchRespondWithObserver* respondWithObserver, WaitUntilObserver* waitUntilObserver)
{
    return new ForeignFetchEvent(scriptState, type, initializer, respondWithObserver, waitUntilObserver);
}

Request* ForeignFetchEvent::request() const
{
    return m_request;
}

String ForeignFetchEvent::origin() const
{
    return m_origin;
}

void ForeignFetchEvent::respondWith(ScriptState* scriptState, ScriptPromise scriptPromise, ExceptionState& exceptionState)
{
    stopImmediatePropagation();
    if (m_observer)
        m_observer->respondWith(scriptState, scriptPromise, exceptionState);
}

const AtomicString& ForeignFetchEvent::interfaceName() const
{
    return EventNames::ForeignFetchEvent;
}

ForeignFetchEvent::ForeignFetchEvent()
{
}

ForeignFetchEvent::ForeignFetchEvent(ScriptState* scriptState, const AtomicString& type, const ForeignFetchEventInit& initializer, ForeignFetchRespondWithObserver* respondWithObserver, WaitUntilObserver* waitUntilObserver)
    : ExtendableEvent(type, initializer, waitUntilObserver)
    , m_observer(respondWithObserver)
{
    if (initializer.hasOrigin())
        m_origin = initializer.origin();
    if (initializer.hasRequest()) {
        m_request = initializer.request();
        ScriptState::Scope scope(scriptState);
        m_request = initializer.request();
        v8::Local<v8::Value> request = toV8(m_request, scriptState);
        v8::Local<v8::Value> event = toV8(this, scriptState);
        if (event.IsEmpty()) {
            // |toV8| can return an empty handle when the worker is terminating.
            // We don't want the renderer to crash in such cases.
            // TODO(yhirano): Replace this branch with an assertion when the
            // graceful shutdown mechanism is introduced.
            return;
        }
        DCHECK(event->IsObject());
        // Sets a hidden value in order to teach V8 the dependency from
        // the event to the request.
        V8HiddenValue::setHiddenValue(scriptState, event.As<v8::Object>(), V8HiddenValue::requestInFetchEvent(scriptState->isolate()), request);
        // From the same reason as above, setHiddenValue can return false.
        // TODO(yhirano): Add an assertion that it returns true once the
        // graceful shutdown mechanism is introduced.
    }
}

DEFINE_TRACE(ForeignFetchEvent)
{
    visitor->trace(m_observer);
    visitor->trace(m_request);
    ExtendableEvent::trace(visitor);
}

} // namespace blink
