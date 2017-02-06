// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerContainerClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include <memory>

namespace blink {

ServiceWorkerContainerClient* ServiceWorkerContainerClient::create(std::unique_ptr<WebServiceWorkerProvider> provider)
{
    return new ServiceWorkerContainerClient(std::move(provider));
}

ServiceWorkerContainerClient::ServiceWorkerContainerClient(std::unique_ptr<WebServiceWorkerProvider> provider)
    : m_provider(std::move(provider))
{
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
    if (context->isWorkerGlobalScope()) {
        WorkerClients* clients = toWorkerGlobalScope(context)->clients();
        ASSERT(clients);
        return static_cast<ServiceWorkerContainerClient*>(Supplement<WorkerClients>::from(clients, supplementName()));
    }
    Document* document = toDocument(context);
    if (!document->frame())
        return nullptr;

    ServiceWorkerContainerClient* client = static_cast<ServiceWorkerContainerClient*>(Supplement<Document>::from(document, supplementName()));
    if (!client) {
        client = new ServiceWorkerContainerClient(document->frame()->loader().client()->createServiceWorkerProvider());
        Supplement<Document>::provideTo(*document, supplementName(), client);
    }
    return client;
}

void provideServiceWorkerContainerClientToWorker(WorkerClients* clients, std::unique_ptr<WebServiceWorkerProvider> provider)
{
    clients->provideSupplement(ServiceWorkerContainerClient::supplementName(), ServiceWorkerContainerClient::create(std::move(provider)));
}

} // namespace blink
