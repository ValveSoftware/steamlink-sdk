// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/WaitUntilObserver.h"

#include "bindings/v8/ScriptFunction.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebServiceWorkerEventResult.h"
#include "wtf/Assertions.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace WebCore {

class WaitUntilObserver::ThenFunction FINAL : public ScriptFunction {
public:
    enum ResolveType {
        Fulfilled,
        Rejected,
    };

    static PassOwnPtr<ScriptFunction> create(PassRefPtr<WaitUntilObserver> observer, ResolveType type)
    {
        ExecutionContext* executionContext = observer->executionContext();
        return adoptPtr(new ThenFunction(toIsolate(executionContext), observer, type));
    }

private:
    ThenFunction(v8::Isolate* isolate, PassRefPtr<WaitUntilObserver> observer, ResolveType type)
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
            m_observer->reportError(value);
        m_observer->decrementPendingActivity();
        m_observer = nullptr;
        return value;
    }

    RefPtr<WaitUntilObserver> m_observer;
    ResolveType m_resolveType;
};

PassRefPtr<WaitUntilObserver> WaitUntilObserver::create(ExecutionContext* context, EventType type, int eventID)
{
    return adoptRef(new WaitUntilObserver(context, type, eventID));
}

WaitUntilObserver::~WaitUntilObserver()
{
    ASSERT(!m_pendingActivity);
}

void WaitUntilObserver::willDispatchEvent()
{
    incrementPendingActivity();
}

void WaitUntilObserver::didDispatchEvent()
{
    decrementPendingActivity();
}

void WaitUntilObserver::waitUntil(ScriptState* scriptState, const ScriptValue& value)
{
    incrementPendingActivity();
    ScriptPromise::cast(scriptState, value).then(
        ThenFunction::create(this, ThenFunction::Fulfilled),
        ThenFunction::create(this, ThenFunction::Rejected));
}

WaitUntilObserver::WaitUntilObserver(ExecutionContext* context, EventType type, int eventID)
    : ContextLifecycleObserver(context)
    , m_type(type)
    , m_eventID(eventID)
    , m_pendingActivity(0)
    , m_hasError(false)
{
}

void WaitUntilObserver::reportError(const ScriptValue& value)
{
    // FIXME: Propagate error message to the client for onerror handling.
    notImplemented();

    m_hasError = true;
}

void WaitUntilObserver::incrementPendingActivity()
{
    ++m_pendingActivity;
}

void WaitUntilObserver::decrementPendingActivity()
{
    ASSERT(m_pendingActivity > 0);
    if (--m_pendingActivity || !executionContext())
        return;

    ServiceWorkerGlobalScopeClient* client = ServiceWorkerGlobalScopeClient::from(executionContext());
    blink::WebServiceWorkerEventResult result = m_hasError ? blink::WebServiceWorkerEventResultRejected : blink::WebServiceWorkerEventResultCompleted;
    switch (m_type) {
    case Activate:
        client->didHandleActivateEvent(m_eventID, result);
        break;
    case Install:
        client->didHandleInstallEvent(m_eventID, result);
        break;
    }
    observeContext(0);
}

} // namespace WebCore
