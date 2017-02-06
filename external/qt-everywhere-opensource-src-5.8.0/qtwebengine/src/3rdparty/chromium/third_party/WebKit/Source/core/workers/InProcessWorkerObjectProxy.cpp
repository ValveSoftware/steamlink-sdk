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

#include "core/workers/InProcessWorkerObjectProxy.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<InProcessWorkerObjectProxy> InProcessWorkerObjectProxy::create(InProcessWorkerMessagingProxy* messagingProxy)
{
    DCHECK(messagingProxy);
    return wrapUnique(new InProcessWorkerObjectProxy(messagingProxy));
}

void InProcessWorkerObjectProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, std::unique_ptr<MessagePortChannelArray> channels)
{
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::postMessageToWorkerObject, crossThreadUnretained(m_messagingProxy), message, passed(std::move(channels))));
}

void InProcessWorkerObjectProxy::postTaskToMainExecutionContext(std::unique_ptr<ExecutionContextTask> task)
{
    getExecutionContext()->postTask(BLINK_FROM_HERE, std::move(task));
}

void InProcessWorkerObjectProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject, crossThreadUnretained(m_messagingProxy), hasPendingActivity));
}

void InProcessWorkerObjectProxy::reportPendingActivity(bool hasPendingActivity)
{
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::reportPendingActivity, crossThreadUnretained(m_messagingProxy), hasPendingActivity));
}

void InProcessWorkerObjectProxy::reportException(const String& errorMessage, std::unique_ptr<SourceLocation> location)
{
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::reportException, crossThreadUnretained(m_messagingProxy), errorMessage, passed(std::move(location))));
}

void InProcessWorkerObjectProxy::reportConsoleMessage(ConsoleMessage* consoleMessage)
{
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::reportConsoleMessage, crossThreadUnretained(m_messagingProxy), consoleMessage->source(), consoleMessage->level(), consoleMessage->message(), passed(consoleMessage->location()->clone())));
}

void InProcessWorkerObjectProxy::postMessageToPageInspector(const String& message)
{
    ExecutionContext* context = getExecutionContext();
    if (context->isDocument())
        toDocument(context)->postInspectorTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::postMessageToPageInspector, crossThreadUnretained(m_messagingProxy), message));
}

void InProcessWorkerObjectProxy::postWorkerConsoleAgentEnabled()
{
    ExecutionContext* context = getExecutionContext();
    if (context->isDocument())
        toDocument(context)->postInspectorTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::postWorkerConsoleAgentEnabled, crossThreadUnretained(m_messagingProxy)));
}

void InProcessWorkerObjectProxy::workerGlobalScopeClosed()
{
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::terminateWorkerGlobalScope, crossThreadUnretained(m_messagingProxy)));
}

void InProcessWorkerObjectProxy::workerThreadTerminated()
{
    // This will terminate the MessagingProxy.
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::workerThreadTerminated, crossThreadUnretained(m_messagingProxy)));
}

InProcessWorkerObjectProxy::InProcessWorkerObjectProxy(InProcessWorkerMessagingProxy* messagingProxy)
    : m_messagingProxy(messagingProxy)
{
}

ExecutionContext* InProcessWorkerObjectProxy::getExecutionContext()
{
    DCHECK(m_messagingProxy);
    return m_messagingProxy->getExecutionContext();
}

} // namespace blink
