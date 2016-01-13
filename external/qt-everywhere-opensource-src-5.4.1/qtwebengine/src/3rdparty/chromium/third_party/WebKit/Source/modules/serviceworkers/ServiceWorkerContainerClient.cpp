// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ServiceWorkerContainerClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/WebServiceWorkerProvider.h"

namespace WebCore {

PassOwnPtrWillBeRawPtr<ServiceWorkerContainerClient> ServiceWorkerContainerClient::create(PassOwnPtr<blink::WebServiceWorkerProvider> provider)
{
    return adoptPtrWillBeNoop(new ServiceWorkerContainerClient(provider));
}

ServiceWorkerContainerClient::~ServiceWorkerContainerClient()
{
}

const char* ServiceWorkerContainerClient::supplementName()
{
    return "ServiceWorkerContainerClient";
}

ServiceWorkerContainerClient* ServiceWorkerContainerClient::from(ExecutionContext* context)
{
    if (context->isDocument()) {
        Document* document = toDocument(context);
        if (!document->frame())
            return 0;

        ServiceWorkerContainerClient* client = static_cast<ServiceWorkerContainerClient*>(DocumentSupplement::from(document, supplementName()));
        if (client)
            return client;

        // If it's not provided yet, create it lazily.
        document->DocumentSupplementable::provideSupplement(ServiceWorkerContainerClient::supplementName(), ServiceWorkerContainerClient::create(document->frame()->loader().client()->createServiceWorkerProvider()));
        return static_cast<ServiceWorkerContainerClient*>(DocumentSupplement::from(document, supplementName()));
    }

    ASSERT(context->isWorkerGlobalScope());
    return static_cast<ServiceWorkerContainerClient*>(WillBeHeapSupplement<WorkerClients>::from(toWorkerGlobalScope(context)->clients(), supplementName()));
}

ServiceWorkerContainerClient::ServiceWorkerContainerClient(PassOwnPtr<blink::WebServiceWorkerProvider> provider)
    : m_provider(provider)
{
}

void provideServiceWorkerContainerClientToWorker(WorkerClients* clients, PassOwnPtr<blink::WebServiceWorkerProvider> provider)
{
    clients->provideSupplement(ServiceWorkerContainerClient::supplementName(), ServiceWorkerContainerClient::create(provider));
}

} // namespace WebCore
