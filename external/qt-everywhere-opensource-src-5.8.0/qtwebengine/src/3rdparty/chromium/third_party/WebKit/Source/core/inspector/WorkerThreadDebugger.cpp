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
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include <v8.h>

namespace blink {

static const int workerContextGroupId = 1;

WorkerThreadDebugger* WorkerThreadDebugger::from(v8::Isolate* isolate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (!data->threadDebugger())
        return nullptr;
    ASSERT(data->threadDebugger()->isWorker());
    return static_cast<WorkerThreadDebugger*>(data->threadDebugger());
}

WorkerThreadDebugger::WorkerThreadDebugger(WorkerThread* workerThread, v8::Isolate* isolate)
    : ThreadDebugger(isolate)
    , m_workerThread(workerThread)
{
}

WorkerThreadDebugger::~WorkerThreadDebugger()
{
}

void WorkerThreadDebugger::contextCreated(v8::Local<v8::Context> context)
{
    debugger()->contextCreated(V8ContextInfo(context, workerContextGroupId, true, m_workerThread->workerGlobalScope()->url().getString(), "", "", false));
}

void WorkerThreadDebugger::contextWillBeDestroyed(v8::Local<v8::Context> context)
{
    debugger()->contextDestroyed(context);
}

int WorkerThreadDebugger::contextGroupId()
{
    return workerContextGroupId;
}

void WorkerThreadDebugger::runMessageLoopOnPause(int contextGroupId)
{
    ASSERT(contextGroupId == workerContextGroupId);
    m_workerThread->startRunningDebuggerTasksOnPauseOnWorkerThread();
}

void WorkerThreadDebugger::quitMessageLoopOnPause()
{
    m_workerThread->stopRunningDebuggerTasksOnPauseOnWorkerThread();
}

bool WorkerThreadDebugger::callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target)
{
    return true;
}

v8::Local<v8::Context> WorkerThreadDebugger::ensureDefaultContextInGroup(int contextGroupId)
{
    ASSERT(contextGroupId == workerContextGroupId);
    ScriptState* scriptState = m_workerThread->workerGlobalScope()->scriptController()->getScriptState();
    return scriptState ? scriptState->context() : v8::Local<v8::Context>();
}

void WorkerThreadDebugger::messageAddedToConsole(int contextGroupId, MessageSource source, MessageLevel level, const String16& message, const String16& url, unsigned lineNumber, unsigned columnNumber, V8StackTrace* stackTrace)
{
    DCHECK(contextGroupId == workerContextGroupId);
    ConsoleMessage* consoleMessage = ConsoleMessage::create(source, level, message, SourceLocation::create(url, lineNumber, columnNumber, stackTrace ? stackTrace->clone() : nullptr, 0));
    m_workerThread->workerReportingProxy().reportConsoleMessage(consoleMessage);
}

v8::MaybeLocal<v8::Value> WorkerThreadDebugger::memoryInfo(v8::Isolate*, v8::Local<v8::Context>)
{
    ASSERT_NOT_REACHED();
    return v8::MaybeLocal<v8::Value>();
}

} // namespace blink
