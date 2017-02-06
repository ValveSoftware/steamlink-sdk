// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/SyncCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/background_sync/SyncError.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

SyncRegistrationCallbacks::SyncRegistrationCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncRegistrationCallbacks::~SyncRegistrationCallbacks()
{
}

void SyncRegistrationCallbacks::onSuccess(std::unique_ptr<WebSyncRegistration> webSyncRegistration)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped()) {
        return;
    }

    std::unique_ptr<WebSyncRegistration> registration = wrapUnique(webSyncRegistration.release());
    if (!registration) {
        m_resolver->resolve(v8::Null(m_resolver->getScriptState()->isolate()));
        return;
    }
    m_resolver->resolve();
}

void SyncRegistrationCallbacks::onError(const WebSyncError& error)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

SyncGetRegistrationsCallbacks::SyncGetRegistrationsCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncGetRegistrationsCallbacks::~SyncGetRegistrationsCallbacks()
{
}

void SyncGetRegistrationsCallbacks::onSuccess(const WebVector<WebSyncRegistration*>& webSyncRegistrations)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    Vector<String> tags;
    for (const WebSyncRegistration* r : webSyncRegistrations) {
        tags.append(r->tag);
    }
    m_resolver->resolve(tags);
}

void SyncGetRegistrationsCallbacks::onError(const WebSyncError& error)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

} // namespace blink
