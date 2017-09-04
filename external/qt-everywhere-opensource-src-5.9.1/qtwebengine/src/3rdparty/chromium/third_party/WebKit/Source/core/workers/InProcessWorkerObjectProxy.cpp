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
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

const double kDefaultIntervalInSec = 1;
const double kMaxIntervalInSec = 30;

std::unique_ptr<InProcessWorkerObjectProxy> InProcessWorkerObjectProxy::create(
    const WeakPtr<InProcessWorkerMessagingProxy>& messagingProxy) {
  DCHECK(messagingProxy);
  return wrapUnique(new InProcessWorkerObjectProxy(messagingProxy));
}

InProcessWorkerObjectProxy::~InProcessWorkerObjectProxy() {}

void InProcessWorkerObjectProxy::postMessageToWorkerObject(
    PassRefPtr<SerializedScriptValue> message,
    std::unique_ptr<MessagePortChannelArray> channels) {
  getParentFrameTaskRunners()
      ->get(TaskType::PostedMessage)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(
                     &InProcessWorkerMessagingProxy::postMessageToWorkerObject,
                     m_messagingProxyWeakPtr, std::move(message),
                     passed(std::move(channels))));
}

void InProcessWorkerObjectProxy::postTaskToMainExecutionContext(
    std::unique_ptr<ExecutionContextTask> task) {
  // TODO(hiroshige,yuryu): Make this not use ExecutionContextTask and use
  // getParentFrameTaskRunners() instead.
  getExecutionContext()->postTask(BLINK_FROM_HERE, std::move(task));
}

void InProcessWorkerObjectProxy::confirmMessageFromWorkerObject() {
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(
              &InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject,
              m_messagingProxyWeakPtr));
}

void InProcessWorkerObjectProxy::startPendingActivityTimer() {
  if (m_timer->isActive()) {
    // Reset the next interval duration to check new activity state timely.
    // For example, a long-running activity can be cancelled by a message
    // event.
    m_nextIntervalInSec = kDefaultIntervalInSec;
    return;
  }
  m_timer->startOneShot(m_nextIntervalInSec, BLINK_FROM_HERE);
  m_nextIntervalInSec = std::min(m_nextIntervalInSec * 1.5, m_maxIntervalInSec);
}

void InProcessWorkerObjectProxy::reportException(
    const String& errorMessage,
    std::unique_ptr<SourceLocation> location,
    int exceptionId) {
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&InProcessWorkerMessagingProxy::dispatchErrorEvent,
                          m_messagingProxyWeakPtr, errorMessage,
                          passed(location->clone()), exceptionId));
}

void InProcessWorkerObjectProxy::reportConsoleMessage(
    MessageSource source,
    MessageLevel level,
    const String& message,
    SourceLocation* location) {
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&InProcessWorkerMessagingProxy::reportConsoleMessage,
                          m_messagingProxyWeakPtr, source, level, message,
                          passed(location->clone())));
}

void InProcessWorkerObjectProxy::postMessageToPageInspector(
    const String& message) {
  ExecutionContext* context = getExecutionContext();
  if (context->isDocument()) {
    // TODO(hiroshige): consider using getParentFrameTaskRunners() here
    // too.
    toDocument(context)->postInspectorTask(
        BLINK_FROM_HERE,
        createCrossThreadTask(
            &InProcessWorkerMessagingProxy::postMessageToPageInspector,
            m_messagingProxyWeakPtr, message));
  }
}

void InProcessWorkerObjectProxy::didCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* globalScope) {
  DCHECK(!m_workerGlobalScope);
  m_workerGlobalScope = toWorkerGlobalScope(globalScope);
  m_timer = wrapUnique(new Timer<InProcessWorkerObjectProxy>(
      this, &InProcessWorkerObjectProxy::checkPendingActivity));
}

void InProcessWorkerObjectProxy::didEvaluateWorkerScript(bool) {
  startPendingActivityTimer();
}

void InProcessWorkerObjectProxy::didCloseWorkerGlobalScope() {
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&InProcessWorkerMessagingProxy::terminateGlobalScope,
                          m_messagingProxyWeakPtr));
}

void InProcessWorkerObjectProxy::willDestroyWorkerGlobalScope() {
  m_timer.reset();
  m_workerGlobalScope = nullptr;
}

void InProcessWorkerObjectProxy::didTerminateWorkerThread() {
  // This will terminate the MessagingProxy.
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(
                     &InProcessWorkerMessagingProxy::workerThreadTerminated,
                     m_messagingProxyWeakPtr));
}

InProcessWorkerObjectProxy::InProcessWorkerObjectProxy(
    const WeakPtr<InProcessWorkerMessagingProxy>& messagingProxy)
    : m_messagingProxy(messagingProxy.get()),
      m_messagingProxyWeakPtr(messagingProxy),
      m_defaultIntervalInSec(kDefaultIntervalInSec),
      m_nextIntervalInSec(kDefaultIntervalInSec),
      m_maxIntervalInSec(kMaxIntervalInSec) {}

ParentFrameTaskRunners*
InProcessWorkerObjectProxy::getParentFrameTaskRunners() {
  DCHECK(m_messagingProxy);
  return m_messagingProxy->getParentFrameTaskRunners();
}

ExecutionContext* InProcessWorkerObjectProxy::getExecutionContext() {
  DCHECK(m_messagingProxy);
  return m_messagingProxy->getExecutionContext();
}

void InProcessWorkerObjectProxy::checkPendingActivity(TimerBase*) {
  bool hasPendingActivity = V8GCController::hasPendingActivity(
      m_workerGlobalScope->thread()->isolate(), m_workerGlobalScope);
  if (!hasPendingActivity) {
    // Report all activities are done.
    getParentFrameTaskRunners()
        ->get(TaskType::Internal)
        ->postTask(BLINK_FROM_HERE,
                   crossThreadBind(
                       &InProcessWorkerMessagingProxy::pendingActivityFinished,
                       m_messagingProxyWeakPtr));

    // Don't schedule a timer. It will be started again when a message event
    // is dispatched.
    m_nextIntervalInSec = m_defaultIntervalInSec;
    return;
  }

  // There is still a pending activity. Check it later.
  startPendingActivityTimer();
}

}  // namespace blink
