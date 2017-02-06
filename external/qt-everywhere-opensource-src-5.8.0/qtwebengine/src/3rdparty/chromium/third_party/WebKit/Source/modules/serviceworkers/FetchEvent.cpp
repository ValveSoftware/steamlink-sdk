// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/FetchEvent.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "wtf/RefPtr.h"

namespace blink {

FetchEvent* FetchEvent::create()
{
    return new FetchEvent();
}

FetchEvent* FetchEvent::create(ScriptState* scriptState, const AtomicString& type, const FetchEventInit& initializer)
{
    return new FetchEvent(scriptState, type, initializer, nullptr, nullptr);
}

FetchEvent* FetchEvent::create(ScriptState* scriptState, const AtomicString& type, const FetchEventInit& initializer, RespondWithObserver* respondWithObserver, WaitUntilObserver* waitUntilObserver)
{
    return new FetchEvent(scriptState, type, initializer, respondWithObserver, waitUntilObserver);
}

Request* FetchEvent::request() const
{
    return m_request;
}

String FetchEvent::clientId() const
{
    return m_clientId;
}

bool FetchEvent::isReload() const
{
    return m_isReload;
}

void FetchEvent::respondWith(ScriptState* scriptState, ScriptPromise scriptPromise, ExceptionState& exceptionState)
{
    stopImmediatePropagation();
    if (m_observer)
        m_observer->respondWith(scriptState, scriptPromise, exceptionState);
}

const AtomicString& FetchEvent::interfaceName() const
{
    return EventNames::FetchEvent;
}

FetchEvent::FetchEvent()
    : m_isReload(false)
{
}

FetchEvent::FetchEvent(ScriptState* scriptState, const AtomicString& type, const FetchEventInit& initializer, RespondWithObserver* respondWithObserver, WaitUntilObserver* waitUntilObserver)
    : ExtendableEvent(type, initializer, waitUntilObserver)
    , m_observer(respondWithObserver)
{
    m_clientId = initializer.clientId();
    m_isReload = initializer.isReload();
    if (initializer.hasRequest()) {
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

DEFINE_TRACE(FetchEvent)
{
    visitor->trace(m_observer);
    visitor->trace(m_request);
    ExtendableEvent::trace(visitor);
}

} // namespace blink
