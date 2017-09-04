// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletGlobalScope.h"

#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"

namespace blink {

ThreadedWorkletGlobalScope::ThreadedWorkletGlobalScope(
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : WorkletGlobalScope(url, userAgent, std::move(securityOrigin), isolate),
      m_thread(thread) {}

ThreadedWorkletGlobalScope::~ThreadedWorkletGlobalScope() {
  DCHECK(!m_thread);
}

void ThreadedWorkletGlobalScope::dispose() {
  DCHECK(isContextThread());
  WorkletGlobalScope::dispose();
  m_thread = nullptr;
}

bool ThreadedWorkletGlobalScope::isContextThread() const {
  return thread()->isCurrentThread();
}

void ThreadedWorkletGlobalScope::postTask(
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task,
    const String& taskNameForInstrumentation) {
  thread()->postTask(location, std::move(task),
                     !taskNameForInstrumentation.isEmpty());
}

void ThreadedWorkletGlobalScope::addConsoleMessage(
    ConsoleMessage* consoleMessage) {
  DCHECK(isContextThread());
  thread()->workerReportingProxy().reportConsoleMessage(
      consoleMessage->source(), consoleMessage->level(),
      consoleMessage->message(), consoleMessage->location());
  thread()->consoleMessageStorage()->addConsoleMessage(this, consoleMessage);
}

void ThreadedWorkletGlobalScope::exceptionThrown(ErrorEvent* errorEvent) {
  DCHECK(isContextThread());
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::from(thread()->isolate()))
    debugger->exceptionThrown(m_thread, errorEvent);
}

}  // namespace blink
