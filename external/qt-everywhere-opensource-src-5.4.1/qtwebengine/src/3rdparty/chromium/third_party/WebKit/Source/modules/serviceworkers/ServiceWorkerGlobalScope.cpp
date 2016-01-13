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
#include "config.h"
#include "ServiceWorkerGlobalScope.h"

#include "CachePolyfill.h"
#include "CacheStoragePolyfill.h"
#include "FetchPolyfill.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8ThrowException.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/FetchManager.h"
#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURL.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<ServiceWorkerGlobalScope> ServiceWorkerGlobalScope::create(ServiceWorkerThread* thread, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
{
    RefPtrWillBeRawPtr<ServiceWorkerGlobalScope> context = adoptRefWillBeRefCountedGarbageCollected(new ServiceWorkerGlobalScope(startupData->m_scriptURL, startupData->m_userAgent, thread, monotonicallyIncreasingTime(), startupData->m_workerClients.release()));

    context->applyContentSecurityPolicyFromString(startupData->m_contentSecurityPolicy, startupData->m_contentSecurityPolicyType);

    context->script()->evaluate(String(fetchPolyfillJs, sizeof(fetchPolyfillJs)));
    context->script()->evaluate(String(cachePolyfillJs, sizeof(cachePolyfillJs)));
    context->script()->evaluate(String(cacheStoragePolyfillJs, sizeof(cacheStoragePolyfillJs)));

    return context.release();
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(const KURL& url, const String& userAgent, ServiceWorkerThread* thread, double timeOrigin, PassOwnPtrWillBeRawPtr<WorkerClients> workerClients)
    : WorkerGlobalScope(url, userAgent, thread, timeOrigin, workerClients)
    , m_fetchManager(adoptPtr(new FetchManager(this)))
{
    ScriptWrappable::init(this);
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope()
{
}

void ServiceWorkerGlobalScope::stopFetch()
{
    m_fetchManager.clear();
}

String ServiceWorkerGlobalScope::scope(ExecutionContext* context)
{
    return ServiceWorkerGlobalScopeClient::from(context)->scope().string();
}

ScriptPromise ServiceWorkerGlobalScope::fetch(ScriptState* scriptState, Request* request)
{
    OwnPtr<ResourceRequest> resourceRequest(request->createResourceRequest());
    return m_fetchManager->fetch(scriptState, resourceRequest.release());
}

ScriptPromise ServiceWorkerGlobalScope::fetch(ScriptState* scriptState, const String& urlstring)
{
    KURL url = completeURL(urlstring);
    if (!url.isValid())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError("Invalid URL", scriptState->isolate()));
    OwnPtr<ResourceRequest> resourceRequest = adoptPtr(new ResourceRequest(url));
    resourceRequest->setHTTPMethod("GET");
    return m_fetchManager->fetch(scriptState, resourceRequest.release());
}

PassRefPtr<ServiceWorkerClients> ServiceWorkerGlobalScope::clients()
{
    if (!m_clients)
        m_clients = ServiceWorkerClients::create();
    return m_clients;
}

const AtomicString& ServiceWorkerGlobalScope::interfaceName() const
{
    return EventTargetNames::ServiceWorkerGlobalScope;
}

void ServiceWorkerGlobalScope::trace(Visitor* visitor)
{
    WorkerGlobalScope::trace(visitor);
}

} // namespace WebCore
