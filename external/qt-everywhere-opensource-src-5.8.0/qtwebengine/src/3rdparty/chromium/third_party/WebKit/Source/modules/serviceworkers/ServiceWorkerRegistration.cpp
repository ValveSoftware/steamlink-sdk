// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerRegistration.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

const AtomicString& ServiceWorkerRegistration::interfaceName() const
{
    return EventTargetNames::ServiceWorkerRegistration;
}

void ServiceWorkerRegistration::dispatchUpdateFoundEvent()
{
    dispatchEvent(Event::create(EventTypeNames::updatefound));
}

void ServiceWorkerRegistration::setInstalling(std::unique_ptr<WebServiceWorker::Handle> handle)
{
    if (!getExecutionContext())
        return;
    m_installing = ServiceWorker::from(getExecutionContext(), wrapUnique(handle.release()));
}

void ServiceWorkerRegistration::setWaiting(std::unique_ptr<WebServiceWorker::Handle> handle)
{
    if (!getExecutionContext())
        return;
    m_waiting = ServiceWorker::from(getExecutionContext(), wrapUnique(handle.release()));
}

void ServiceWorkerRegistration::setActive(std::unique_ptr<WebServiceWorker::Handle> handle)
{
    if (!getExecutionContext())
        return;
    m_active = ServiceWorker::from(getExecutionContext(), wrapUnique(handle.release()));
}

ServiceWorkerRegistration* ServiceWorkerRegistration::getOrCreate(ExecutionContext* executionContext, std::unique_ptr<WebServiceWorkerRegistration::Handle> handle)
{
    ASSERT(handle);

    ServiceWorkerRegistration* existingRegistration = static_cast<ServiceWorkerRegistration*>(handle->registration()->proxy());
    if (existingRegistration) {
        ASSERT(existingRegistration->getExecutionContext() == executionContext);
        return existingRegistration;
    }

    ServiceWorkerRegistration* newRegistration = new ServiceWorkerRegistration(executionContext, std::move(handle));
    newRegistration->suspendIfNeeded();
    return newRegistration;
}

String ServiceWorkerRegistration::scope() const
{
    return m_handle->registration()->scope().string();
}

ScriptPromise ServiceWorkerRegistration::update(ScriptState* scriptState)
{
    ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(getExecutionContext());
    if (!client || !client->provider())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Failed to update a ServiceWorkerRegistration: No associated provider is available."));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_handle->registration()->update(client->provider(), new CallbackPromiseAdapter<void, ServiceWorkerError>(resolver));
    return promise;
}

ScriptPromise ServiceWorkerRegistration::unregister(ScriptState* scriptState)
{
    ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(getExecutionContext());
    if (!client || !client->provider())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Failed to unregister a ServiceWorkerRegistration: No associated provider is available."));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_handle->registration()->unregister(client->provider(), new CallbackPromiseAdapter<bool, ServiceWorkerError>(resolver));
    return promise;
}

ServiceWorkerRegistration::ServiceWorkerRegistration(ExecutionContext* executionContext, std::unique_ptr<WebServiceWorkerRegistration::Handle> handle)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(executionContext)
    , m_handle(std::move(handle))
    , m_stopped(false)
{
    ASSERT(m_handle);
    ASSERT(!m_handle->registration()->proxy());
    ThreadState::current()->registerPreFinalizer(this);

    if (!executionContext)
        return;
    m_handle->registration()->setProxy(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration()
{
}

void ServiceWorkerRegistration::dispose()
{
    // Promptly clears a raw reference from content/ to an on-heap object
    // so that content/ doesn't access it in a lazy sweeping phase.
    m_handle.reset();
}

DEFINE_TRACE(ServiceWorkerRegistration)
{
    visitor->trace(m_installing);
    visitor->trace(m_waiting);
    visitor->trace(m_active);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
    Supplementable<ServiceWorkerRegistration>::trace(visitor);
}

bool ServiceWorkerRegistration::hasPendingActivity() const
{
    return !m_stopped;
}

void ServiceWorkerRegistration::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_handle->registration()->proxyStopped();
}

} // namespace blink
