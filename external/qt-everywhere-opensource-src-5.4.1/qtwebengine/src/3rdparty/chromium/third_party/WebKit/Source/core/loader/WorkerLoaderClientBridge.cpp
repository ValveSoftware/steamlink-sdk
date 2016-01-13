/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "core/loader/WorkerLoaderClientBridge.h"

#include "core/dom/CrossThreadTask.h"
#include "core/loader/ThreadableLoaderClientWrapper.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

PassOwnPtr<ThreadableLoaderClient> WorkerLoaderClientBridge::create(PassRefPtr<ThreadableLoaderClientWrapper> client, WorkerLoaderProxy& loaderProxy)
{
    return adoptPtr(new WorkerLoaderClientBridge(client, loaderProxy));
}

WorkerLoaderClientBridge::~WorkerLoaderClientBridge()
{
}

static void workerGlobalScopeDidSendData(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didSendData(bytesSent, totalBytesToBeSent);
}

void WorkerLoaderClientBridge::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidSendData, m_workerClientWrapper, bytesSent, totalBytesToBeSent));
}

static void workerGlobalScopeDidReceiveResponse(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, unsigned long identifier, PassOwnPtr<CrossThreadResourceResponseData> responseData)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    OwnPtr<ResourceResponse> response(ResourceResponse::adopt(responseData));
    workerClientWrapper->didReceiveResponse(identifier, *response);
}

void WorkerLoaderClientBridge::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveResponse, m_workerClientWrapper, identifier, response));
}

static void workerGlobalScopeDidReceiveData(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, PassOwnPtr<Vector<char> > vectorData)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveData(vectorData->data(), vectorData->size());
}

void WorkerLoaderClientBridge::didReceiveData(const char* data, int dataLength)
{
    OwnPtr<Vector<char> > vector = adoptPtr(new Vector<char>(dataLength)); // needs to be an OwnPtr for usage with createCallbackTask.
    memcpy(vector->data(), data, dataLength);
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveData, m_workerClientWrapper, vector.release()));
}

static void workerGlobalScopeDidDownloadData(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, int dataLength)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didDownloadData(dataLength);
}

void WorkerLoaderClientBridge::didDownloadData(int dataLength)
{
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidDownloadData, m_workerClientWrapper, dataLength));
}

static void workerGlobalScopeDidReceiveCachedMetadata(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, PassOwnPtr<Vector<char> > vectorData)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveCachedMetadata(vectorData->data(), vectorData->size());
}

void WorkerLoaderClientBridge::didReceiveCachedMetadata(const char* data, int dataLength)
{
    OwnPtr<Vector<char> > vector = adoptPtr(new Vector<char>(dataLength)); // needs to be an OwnPtr for usage with createCallbackTask.
    memcpy(vector->data(), data, dataLength);
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveCachedMetadata, m_workerClientWrapper, vector.release()));
}

static void workerGlobalScopeDidFinishLoading(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, unsigned long identifier, double finishTime)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didFinishLoading(identifier, finishTime);
}

void WorkerLoaderClientBridge::didFinishLoading(unsigned long identifier, double finishTime)
{
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidFinishLoading, m_workerClientWrapper, identifier, finishTime));
}

static void workerGlobalScopeDidFail(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, const ResourceError& error)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didFail(error);
}

void WorkerLoaderClientBridge::didFail(const ResourceError& error)
{
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidFail, m_workerClientWrapper, error));
}

static void workerGlobalScopeDidFailAccessControlCheck(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, const ResourceError& error)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didFailAccessControlCheck(error);
}

void WorkerLoaderClientBridge::didFailAccessControlCheck(const ResourceError& error)
{
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidFailAccessControlCheck, m_workerClientWrapper, error));
}

static void workerGlobalScopeDidFailRedirectCheck(ExecutionContext* context, PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didFailRedirectCheck();
}

void WorkerLoaderClientBridge::didFailRedirectCheck()
{
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidFailRedirectCheck, m_workerClientWrapper));
}

WorkerLoaderClientBridge::WorkerLoaderClientBridge(PassRefPtr<ThreadableLoaderClientWrapper> clientWrapper, WorkerLoaderProxy& loaderProxy)
    : m_workerClientWrapper(clientWrapper)
    , m_loaderProxy(loaderProxy)
{
}

} // namespace WebCore
