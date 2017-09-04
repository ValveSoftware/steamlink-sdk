/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/WorkerThreadDebugger.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/events/ErrorEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/V8InspectorString.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include <v8.h>

namespace blink {

namespace {

const int kInvalidContextGroupId = 0;

}  // namespace

WorkerThreadDebugger* WorkerThreadDebugger::from(v8::Isolate* isolate) {
  V8PerIsolateData* data = V8PerIsolateData::from(isolate);
  if (!data->threadDebugger())
    return nullptr;
  ASSERT(data->threadDebugger()->isWorker());
  return static_cast<WorkerThreadDebugger*>(data->threadDebugger());
}

WorkerThreadDebugger::WorkerThreadDebugger(v8::Isolate* isolate)
    : ThreadDebugger(isolate), m_pausedContextGroupId(kInvalidContextGroupId) {}

WorkerThreadDebugger::~WorkerThreadDebugger() {
  DCHECK(m_workerThreads.isEmpty());
}

void WorkerThreadDebugger::reportConsoleMessage(ExecutionContext* context,
                                                MessageSource source,
                                                MessageLevel level,
                                                const String& message,
                                                SourceLocation* location) {
  if (!context)
    return;
  toWorkerOrWorkletGlobalScope(context)
      ->thread()
      ->workerReportingProxy()
      .reportConsoleMessage(source, level, message, location);
}

int WorkerThreadDebugger::contextGroupId(WorkerThread* workerThread) {
  return workerThread->getWorkerThreadId();
}

void WorkerThreadDebugger::contextCreated(WorkerThread* workerThread,
                                          v8::Local<v8::Context> context) {
  int workerContextGroupId = contextGroupId(workerThread);
  v8_inspector::V8ContextInfo contextInfo(context, workerContextGroupId,
                                          v8_inspector::StringView());
  String origin = workerThread->globalScope()->url().getString();
  contextInfo.origin = toV8InspectorStringView(origin);
  v8Inspector()->contextCreated(contextInfo);

  DCHECK(!m_workerThreads.contains(workerContextGroupId));
  m_workerThreads.add(workerContextGroupId, workerThread);
}

void WorkerThreadDebugger::contextWillBeDestroyed(
    WorkerThread* workerThread,
    v8::Local<v8::Context> context) {
  int workerContextGroupId = contextGroupId(workerThread);
  DCHECK(m_workerThreads.contains(workerContextGroupId));
  m_workerThreads.remove(workerContextGroupId);
  v8Inspector()->contextDestroyed(context);
}

void WorkerThreadDebugger::exceptionThrown(WorkerThread* workerThread,
                                           ErrorEvent* event) {
  workerThread->workerReportingProxy().reportConsoleMessage(
      JSMessageSource, ErrorMessageLevel, event->messageForConsole(),
      event->location());

  const String defaultMessage = "Uncaught";
  ScriptState* scriptState =
      workerThread->globalScope()->scriptController()->getScriptState();
  if (scriptState && scriptState->contextIsValid()) {
    ScriptState::Scope scope(scriptState);
    v8::Local<v8::Value> exception =
        V8ErrorHandler::loadExceptionFromErrorEventWrapper(
            scriptState, event, scriptState->context()->Global());
    SourceLocation* location = event->location();
    String message = event->messageForConsole();
    String url = location->url();
    v8Inspector()->exceptionThrown(
        scriptState->context(), toV8InspectorStringView(defaultMessage),
        exception, toV8InspectorStringView(message),
        toV8InspectorStringView(url), location->lineNumber(),
        location->columnNumber(), location->takeStackTrace(),
        location->scriptId());
  }
}

int WorkerThreadDebugger::contextGroupId(ExecutionContext* context) {
  return contextGroupId(toWorkerOrWorkletGlobalScope(context)->thread());
}

void WorkerThreadDebugger::runMessageLoopOnPause(int contextGroupId) {
  DCHECK_EQ(kInvalidContextGroupId, m_pausedContextGroupId);
  DCHECK(m_workerThreads.contains(contextGroupId));
  m_pausedContextGroupId = contextGroupId;
  m_workerThreads.get(contextGroupId)
      ->startRunningDebuggerTasksOnPauseOnWorkerThread();
}

void WorkerThreadDebugger::quitMessageLoopOnPause() {
  DCHECK_NE(kInvalidContextGroupId, m_pausedContextGroupId);
  DCHECK(m_workerThreads.contains(m_pausedContextGroupId));
  m_workerThreads.get(m_pausedContextGroupId)
      ->stopRunningDebuggerTasksOnPauseOnWorkerThread();
  m_pausedContextGroupId = kInvalidContextGroupId;
}

void WorkerThreadDebugger::muteMetrics(int contextGroupId) {
  DCHECK(m_workerThreads.contains(contextGroupId));
}

void WorkerThreadDebugger::unmuteMetrics(int contextGroupId) {
  DCHECK(m_workerThreads.contains(contextGroupId));
}

v8::Local<v8::Context> WorkerThreadDebugger::ensureDefaultContextInGroup(
    int contextGroupId) {
  DCHECK(m_workerThreads.contains(contextGroupId));
  ScriptState* scriptState = m_workerThreads.get(contextGroupId)
                                 ->globalScope()
                                 ->scriptController()
                                 ->getScriptState();
  return scriptState ? scriptState->context() : v8::Local<v8::Context>();
}

void WorkerThreadDebugger::beginEnsureAllContextsInGroup(int contextGroupId) {
  DCHECK(m_workerThreads.contains(contextGroupId));
}

void WorkerThreadDebugger::endEnsureAllContextsInGroup(int contextGroupId) {
  DCHECK(m_workerThreads.contains(contextGroupId));
}

bool WorkerThreadDebugger::canExecuteScripts(int contextGroupId) {
  DCHECK(m_workerThreads.contains(contextGroupId));
  return true;
}

void WorkerThreadDebugger::runIfWaitingForDebugger(int contextGroupId) {
  DCHECK(m_workerThreads.contains(contextGroupId));
  m_workerThreads.get(contextGroupId)
      ->stopRunningDebuggerTasksOnPauseOnWorkerThread();
}

void WorkerThreadDebugger::consoleAPIMessage(
    int contextGroupId,
    v8_inspector::V8ConsoleAPIType type,
    const v8_inspector::StringView& message,
    const v8_inspector::StringView& url,
    unsigned lineNumber,
    unsigned columnNumber,
    v8_inspector::V8StackTrace* stackTrace) {
  DCHECK(m_workerThreads.contains(contextGroupId));
  WorkerThread* workerThread = m_workerThreads.get(contextGroupId);

  if (type == v8_inspector::V8ConsoleAPIType::kClear)
    workerThread->consoleMessageStorage()->clear();
  std::unique_ptr<SourceLocation> location =
      SourceLocation::create(toCoreString(url), lineNumber, columnNumber,
                             stackTrace ? stackTrace->clone() : nullptr, 0);
  workerThread->workerReportingProxy().reportConsoleMessage(
      ConsoleAPIMessageSource, consoleAPITypeToMessageLevel(type),
      toCoreString(message), location.get());
}

v8::MaybeLocal<v8::Value> WorkerThreadDebugger::memoryInfo(
    v8::Isolate*,
    v8::Local<v8::Context>) {
  ASSERT_NOT_REACHED();
  return v8::MaybeLocal<v8::Value>();
}

}  // namespace blink
