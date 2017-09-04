// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletObjectProxy.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<ThreadedWorkletObjectProxy> ThreadedWorkletObjectProxy::create(
    ThreadedWorkletMessagingProxy* messagingProxy) {
  DCHECK(messagingProxy);
  return wrapUnique(new ThreadedWorkletObjectProxy(messagingProxy));
}

void ThreadedWorkletObjectProxy::postTaskToMainExecutionContext(
    std::unique_ptr<ExecutionContextTask> task) {
  getExecutionContext()->postTask(BLINK_FROM_HERE, std::move(task));
}

void ThreadedWorkletObjectProxy::reportConsoleMessage(
    MessageSource source,
    MessageLevel level,
    const String& message,
    SourceLocation* location) {
  getExecutionContext()->postTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &::blink::ThreadedWorkletMessagingProxy::reportConsoleMessage,
          crossThreadUnretained(m_messagingProxy), source, level, message,
          passed(location->clone())));
}

void ThreadedWorkletObjectProxy::postMessageToPageInspector(
    const String& message) {
  DCHECK(getExecutionContext()->isDocument());
  toDocument(getExecutionContext())
      ->postInspectorTask(
          BLINK_FROM_HERE,
          createCrossThreadTask(&::blink::ThreadedWorkletMessagingProxy::
                                    postMessageToPageInspector,
                                crossThreadUnretained(m_messagingProxy),
                                message));
}

void ThreadedWorkletObjectProxy::didCloseWorkerGlobalScope() {
  getExecutionContext()->postTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &::blink::ThreadedWorkletMessagingProxy::terminateGlobalScope,
          crossThreadUnretained(m_messagingProxy)));
}

void ThreadedWorkletObjectProxy::didTerminateWorkerThread() {
  // This will terminate the MessagingProxy.
  getExecutionContext()->postTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &ThreadedWorkletMessagingProxy::workerThreadTerminated,
          crossThreadUnretained(m_messagingProxy)));
}

ThreadedWorkletObjectProxy::ThreadedWorkletObjectProxy(
    ThreadedWorkletMessagingProxy* messagingProxy)
    : m_messagingProxy(messagingProxy) {}

ExecutionContext* ThreadedWorkletObjectProxy::getExecutionContext() const {
  DCHECK(m_messagingProxy);
  return m_messagingProxy->getExecutionContext();
}

}  // namespace blink
