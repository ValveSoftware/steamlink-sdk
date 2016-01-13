/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#include "core/loader/WorkerThreadableLoader.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/WorkerLoaderClientBridgeSyncHelper.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/MainThread.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

WorkerThreadableLoader::WorkerThreadableLoader(WorkerGlobalScope& workerGlobalScope, PassRefPtr<ThreadableLoaderClientWrapper> clientWrapper, PassOwnPtr<ThreadableLoaderClient> clientBridge, const ResourceRequest& request, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
    : m_workerGlobalScope(&workerGlobalScope)
    , m_workerClientWrapper(clientWrapper)
    , m_bridge(*(new MainThreadBridge(m_workerClientWrapper, clientBridge, workerGlobalScope.thread()->workerLoaderProxy(), request, options, resourceLoaderOptions, workerGlobalScope.url().strippedForUseAsReferrer())))
{
}

WorkerThreadableLoader::~WorkerThreadableLoader()
{
    m_bridge.destroy();
}

void WorkerThreadableLoader::loadResourceSynchronously(WorkerGlobalScope& workerGlobalScope, const ResourceRequest& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
{
    blink::WebWaitableEvent* shutdownEvent =
        workerGlobalScope.thread()->shutdownEvent();
    OwnPtr<blink::WebWaitableEvent> loaderDone =
        adoptPtr(blink::Platform::current()->createWaitableEvent());

    Vector<blink::WebWaitableEvent*> events;
    events.append(shutdownEvent);
    events.append(loaderDone.get());

    RefPtr<ThreadableLoaderClientWrapper> clientWrapper(ThreadableLoaderClientWrapper::create(&client));
    OwnPtr<WorkerLoaderClientBridgeSyncHelper> clientBridge(WorkerLoaderClientBridgeSyncHelper::create(client, loaderDone.release()));

    // This must be valid while loader is around.
    WorkerLoaderClientBridgeSyncHelper* clientBridgePtr = clientBridge.get();

    RefPtr<WorkerThreadableLoader> loader = WorkerThreadableLoader::create(workerGlobalScope, clientWrapper, clientBridge.release(), request, options, resourceLoaderOptions);

    blink::WebWaitableEvent* signalled;
    {
        ThreadState::SafePointScope scope(ThreadState::HeapPointersOnStack);
        signalled = blink::Platform::current()->waitMultipleEvents(events);
    }
    if (signalled == shutdownEvent) {
        loader->cancel();
        return;
    }

    clientBridgePtr->run();
}

void WorkerThreadableLoader::cancel()
{
    m_bridge.cancel();
}

WorkerThreadableLoader::MainThreadBridge::MainThreadBridge(
    PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper,
    PassOwnPtr<ThreadableLoaderClient> clientBridge,
    WorkerLoaderProxy& loaderProxy,
    const ResourceRequest& request,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions,
    const String& outgoingReferrer)
    : m_clientBridge(clientBridge)
    , m_workerClientWrapper(workerClientWrapper)
    , m_loaderProxy(loaderProxy)
{
    ASSERT(m_workerClientWrapper.get());
    ASSERT(m_clientBridge.get());
    m_loaderProxy.postTaskToLoader(
        createCallbackTask(&MainThreadBridge::mainThreadCreateLoader, AllowCrossThreadAccess(this), request, options, resourceLoaderOptions, outgoingReferrer));
}

WorkerThreadableLoader::MainThreadBridge::~MainThreadBridge()
{
}

void WorkerThreadableLoader::MainThreadBridge::mainThreadCreateLoader(ExecutionContext* context, MainThreadBridge* thisPtr, PassOwnPtr<CrossThreadResourceRequestData> requestData, ThreadableLoaderOptions options, ResourceLoaderOptions resourceLoaderOptions, const String& outgoingReferrer)
{
    ASSERT(isMainThread());
    Document* document = toDocument(context);

    OwnPtr<ResourceRequest> request(ResourceRequest::adopt(requestData));
    request->setHTTPReferrer(Referrer(outgoingReferrer, ReferrerPolicyDefault));
    resourceLoaderOptions.requestInitiatorContext = WorkerContext;
    thisPtr->m_mainThreadLoader = DocumentThreadableLoader::create(*document, thisPtr, *request, options, resourceLoaderOptions);
    if (!thisPtr->m_mainThreadLoader) {
        // DocumentThreadableLoader::create may return 0 when the document loader has been already changed.
        thisPtr->didFail(ResourceError(errorDomainBlinkInternal, 0, request->url().string(), "Can't create DocumentThreadableLoader"));
    }
}

void WorkerThreadableLoader::MainThreadBridge::mainThreadDestroy(ExecutionContext* context, MainThreadBridge* thisPtr)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    delete thisPtr;
}

void WorkerThreadableLoader::MainThreadBridge::destroy()
{
    // Ensure that no more client callbacks are done in the worker context's thread.
    clearClientWrapper();

    // "delete this" and m_mainThreadLoader::deref() on the worker object's thread.
    m_loaderProxy.postTaskToLoader(
        createCallbackTask(&MainThreadBridge::mainThreadDestroy, AllowCrossThreadAccess(this)));
}

void WorkerThreadableLoader::MainThreadBridge::mainThreadCancel(ExecutionContext* context, MainThreadBridge* thisPtr)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());

    if (!thisPtr->m_mainThreadLoader)
        return;
    thisPtr->m_mainThreadLoader->cancel();
    thisPtr->m_mainThreadLoader = nullptr;
}

void WorkerThreadableLoader::MainThreadBridge::cancel()
{
    m_loaderProxy.postTaskToLoader(
        createCallbackTask(&MainThreadBridge::mainThreadCancel, AllowCrossThreadAccess(this)));
    ThreadableLoaderClientWrapper* clientWrapper = m_workerClientWrapper.get();
    if (!clientWrapper->done()) {
        // If the client hasn't reached a termination state, then transition it by sending a cancellation error.
        // Note: no more client callbacks will be done after this method -- the clearClientWrapper() call ensures that.
        ResourceError error(String(), 0, String(), String());
        error.setIsCancellation(true);
        clientWrapper->didFail(error);
    }
    clearClientWrapper();
}

void WorkerThreadableLoader::MainThreadBridge::clearClientWrapper()
{
    m_workerClientWrapper->clearClient();
}

void WorkerThreadableLoader::MainThreadBridge::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_clientBridge->didSendData(bytesSent, totalBytesToBeSent);
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    m_clientBridge->didReceiveResponse(identifier, response);
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveData(const char* data, int dataLength)
{
    m_clientBridge->didReceiveData(data, dataLength);
}

void WorkerThreadableLoader::MainThreadBridge::didDownloadData(int dataLength)
{
    m_clientBridge->didDownloadData(dataLength);
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveCachedMetadata(const char* data, int dataLength)
{
    m_clientBridge->didReceiveCachedMetadata(data, dataLength);
}

void WorkerThreadableLoader::MainThreadBridge::didFinishLoading(unsigned long identifier, double finishTime)
{
    m_clientBridge->didFinishLoading(identifier, finishTime);
}

void WorkerThreadableLoader::MainThreadBridge::didFail(const ResourceError& error)
{
    m_clientBridge->didFail(error);
}

void WorkerThreadableLoader::MainThreadBridge::didFailAccessControlCheck(const ResourceError& error)
{
    m_clientBridge->didFailAccessControlCheck(error);
}

void WorkerThreadableLoader::MainThreadBridge::didFailRedirectCheck()
{
    m_clientBridge->didFailRedirectCheck();
}

} // namespace WebCore
