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
#include "core/workers/WorkerObjectProxy.h"

#include "bindings/v8/SerializedScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerMessagingProxy.h"
#include "platform/NotImplemented.h"
#include "wtf/Functional.h"

namespace WebCore {

PassOwnPtr<WorkerObjectProxy> WorkerObjectProxy::create(ExecutionContext* executionContext, WorkerMessagingProxy* messagingProxy)
{
    return adoptPtr(new WorkerObjectProxy(executionContext, messagingProxy));
}

void WorkerObjectProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
{
    m_executionContext->postTask(bind(&WorkerMessagingProxy::postMessageToWorkerObject, m_messagingProxy, message, channels));
}

void WorkerObjectProxy::postTaskToMainExecutionContext(PassOwnPtr<ExecutionContextTask> task)
{
    m_executionContext->postTask(task);
}

void WorkerObjectProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    m_executionContext->postTask(bind(&WorkerMessagingProxy::confirmMessageFromWorkerObject, m_messagingProxy, hasPendingActivity));
}

void WorkerObjectProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_executionContext->postTask(bind(&WorkerMessagingProxy::reportPendingActivity, m_messagingProxy, hasPendingActivity));
}

void WorkerObjectProxy::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    m_executionContext->postTask(bind(&WorkerMessagingProxy::reportException, m_messagingProxy, errorMessage.isolatedCopy(), lineNumber, columnNumber, sourceURL.isolatedCopy()));
}

void WorkerObjectProxy::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    m_executionContext->postTask(bind(&WorkerMessagingProxy::reportConsoleMessage, m_messagingProxy, source, level, message.isolatedCopy(), lineNumber, sourceURL.isolatedCopy()));
}

void WorkerObjectProxy::postMessageToPageInspector(const String& message)
{
    m_executionContext->postTask(bind(&WorkerMessagingProxy::postMessageToPageInspector, m_messagingProxy, message.isolatedCopy()));
}

void WorkerObjectProxy::updateInspectorStateCookie(const String&)
{
    notImplemented();
}

void WorkerObjectProxy::workerGlobalScopeClosed()
{
    m_executionContext->postTask(bind(&WorkerMessagingProxy::terminateWorkerGlobalScope, m_messagingProxy));
}

void WorkerObjectProxy::workerGlobalScopeDestroyed()
{
    // This will terminate the MessagingProxy.
    m_executionContext->postTask(bind(&WorkerMessagingProxy::workerGlobalScopeDestroyed, m_messagingProxy));
}

WorkerObjectProxy::WorkerObjectProxy(ExecutionContext* executionContext, WorkerMessagingProxy* messagingProxy)
    : m_executionContext(executionContext)
    , m_messagingProxy(messagingProxy)
{
}

} // namespace WebCore
