// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/RespondWithObserver.h"

#include "V8Response.h"
#include "bindings/v8/ScriptFunction.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace WebCore {

class RespondWithObserver::ThenFunction FINAL : public ScriptFunction {
public:
    enum ResolveType {
        Fulfilled,
        Rejected,
    };

    static PassOwnPtr<ScriptFunction> create(PassRefPtr<RespondWithObserver> observer, ResolveType type)
    {
        ExecutionContext* executionContext = observer->executionContext();
        return adoptPtr(new ThenFunction(toIsolate(executionContext), observer, type));
    }

private:
    ThenFunction(v8::Isolate* isolate, PassRefPtr<RespondWithObserver> observer, ResolveType type)
        : ScriptFunction(isolate)
        , m_observer(observer)
        , m_resolveType(type)
    {
    }

    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        ASSERT(m_observer);
        ASSERT(m_resolveType == Fulfilled || m_resolveType == Rejected);
        if (m_resolveType == Rejected)
            m_observer->responseWasRejected();
        else
            m_observer->responseWasFulfilled(value);
        m_observer = nullptr;
        return value;
    }

    RefPtr<RespondWithObserver> m_observer;
    ResolveType m_resolveType;
};

PassRefPtr<RespondWithObserver> RespondWithObserver::create(ExecutionContext* context, int eventID)
{
    return adoptRef(new RespondWithObserver(context, eventID));
}

RespondWithObserver::~RespondWithObserver()
{
    ASSERT(m_state == Done);
}

void RespondWithObserver::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();
    m_state = Done;
}

void RespondWithObserver::didDispatchEvent()
{
    if (m_state == Initial)
        sendResponse(nullptr);
}

void RespondWithObserver::respondWith(ScriptState* scriptState, const ScriptValue& value)
{
    if (m_state != Initial)
        return;

    m_state = Pending;
    ScriptPromise::cast(scriptState, value).then(
        ThenFunction::create(this, ThenFunction::Fulfilled),
        ThenFunction::create(this, ThenFunction::Rejected));
}

void RespondWithObserver::sendResponse(PassRefPtr<Response> response)
{
    if (!executionContext())
        return;
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID, response);
    m_state = Done;
}

void RespondWithObserver::responseWasRejected()
{
    // FIXME: Throw a NetworkError to service worker's execution context.
    sendResponse(nullptr);
}

void RespondWithObserver::responseWasFulfilled(const ScriptValue& value)
{
    if (!executionContext())
        return;
    if (!V8Response::hasInstance(value.v8Value(), toIsolate(executionContext()))) {
        responseWasRejected();
        return;
    }
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value.v8Value());
    sendResponse(V8Response::toNative(object));
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context, int eventID)
    : ContextLifecycleObserver(context)
    , m_eventID(eventID)
    , m_state(Initial)
{
}

} // namespace WebCore
