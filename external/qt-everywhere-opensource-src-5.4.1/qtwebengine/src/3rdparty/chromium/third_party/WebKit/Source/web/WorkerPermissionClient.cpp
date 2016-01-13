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
#include "web/WorkerPermissionClient.h"

#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/WebPermissionCallbacks.h"
#include "public/platform/WebString.h"
#include "public/web/WebWorkerPermissionClientProxy.h"
#include "wtf/PassOwnPtr.h"

using namespace WebCore;

namespace blink {

PassOwnPtrWillBeRawPtr<WorkerPermissionClient> WorkerPermissionClient::create(PassOwnPtr<WebWorkerPermissionClientProxy> proxy)
{
    return adoptPtrWillBeNoop(new WorkerPermissionClient(proxy));
}

WorkerPermissionClient::~WorkerPermissionClient()
{
}

bool WorkerPermissionClient::allowDatabase(const WebString& name, const WebString& displayName, unsigned long estimatedSize)
{
    if (!m_proxy)
        return true;
    return m_proxy->allowDatabase(name, displayName, estimatedSize);
}

bool WorkerPermissionClient::requestFileSystemAccessSync()
{
    if (!m_proxy)
        return true;
    return m_proxy->requestFileSystemAccessSync();
}

bool WorkerPermissionClient::allowIndexedDB(const WebString& name)
{
    if (!m_proxy)
        return true;
    return m_proxy->allowIndexedDB(name);
}

const char* WorkerPermissionClient::supplementName()
{
    return "WorkerPermissionClient";
}

WorkerPermissionClient* WorkerPermissionClient::from(ExecutionContext& context)
{
    WorkerClients* clients = toWorkerGlobalScope(context).clients();
    ASSERT(clients);
    return static_cast<WorkerPermissionClient*>(WillBeHeapSupplement<WorkerClients>::from(*clients, supplementName()));
}

WorkerPermissionClient::WorkerPermissionClient(PassOwnPtr<WebWorkerPermissionClientProxy> proxy)
    : m_proxy(proxy)
{
}

void providePermissionClientToWorker(WorkerClients* clients, PassOwnPtr<WebWorkerPermissionClientProxy> proxy)
{
    ASSERT(clients);
    WorkerPermissionClient::provideTo(*clients, WorkerPermissionClient::supplementName(), WorkerPermissionClient::create(proxy));
}

} // namespace blink
