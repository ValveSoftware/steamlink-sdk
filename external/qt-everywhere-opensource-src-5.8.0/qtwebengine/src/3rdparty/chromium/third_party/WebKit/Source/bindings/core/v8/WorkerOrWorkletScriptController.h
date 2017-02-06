/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#ifndef WorkerOrWorkletScriptController_h
#define WorkerOrWorkletScriptController_h

#include "bindings/core/v8/RejectedPromises.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/CoreExport.h"
#include "platform/text/CompressibleString.h"
#include "wtf/Allocator.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/TextPosition.h"
#include <v8.h>

namespace blink {

class CachedMetadataHandler;
class ErrorEvent;
class ExceptionState;
class ScriptSourceCode;
class WorkerOrWorkletGlobalScope;

class CORE_EXPORT WorkerOrWorkletScriptController : public GarbageCollectedFinalized<WorkerOrWorkletScriptController> {
    WTF_MAKE_NONCOPYABLE(WorkerOrWorkletScriptController);
public:
    static WorkerOrWorkletScriptController* create(WorkerOrWorkletGlobalScope*, v8::Isolate*);
    virtual ~WorkerOrWorkletScriptController();
    void dispose();

    bool isExecutionForbidden() const;
    bool isExecutionTerminating() const;

    // Returns true if the evaluation completed with no uncaught exception.
    bool evaluate(const ScriptSourceCode&, ErrorEvent** = nullptr, CachedMetadataHandler* = nullptr, V8CacheOptions = V8CacheOptionsDefault);

    // Prevents future JavaScript execution. See
    // willScheduleExecutionTermination, isExecutionForbidden.
    void forbidExecution();

    // Used by WorkerThread:
    bool initializeContextIfNeeded();
    // Async request to terminate future JavaScript execution on the worker
    // thread. JavaScript evaluation exits with a non-continuable exception and
    // WorkerOrWorkletScriptController calls forbidExecution to prevent further
    // JavaScript execution. Use forbidExecution()/isExecutionForbidden() to
    // guard against reentry into JavaScript.
    void willScheduleExecutionTermination();

    // Used by WorkerGlobalScope:
    void rethrowExceptionFromImportedScript(ErrorEvent*, ExceptionState&);
    void disableEval(const String&);

    // Used by Inspector agents:
    ScriptState* getScriptState() { return m_scriptState.get(); }

    // Used by V8 bindings:
    v8::Local<v8::Context> context() { return m_scriptState ? m_scriptState->context() : v8::Local<v8::Context>(); }

    RejectedPromises* getRejectedPromises() const { return m_rejectedPromises.get(); }

    DECLARE_TRACE();

    bool isContextInitialized() const { return m_scriptState && !!m_scriptState->perContextData(); }

private:
    WorkerOrWorkletScriptController(WorkerOrWorkletGlobalScope*, v8::Isolate*);
    class ExecutionState;

    // Evaluate a script file in the current execution environment.
    ScriptValue evaluate(const CompressibleString& script, const String& fileName, const TextPosition& scriptStartPosition, CachedMetadataHandler*, V8CacheOptions);
    void disposeContextIfNeeded();

    Member<WorkerOrWorkletGlobalScope> m_globalScope;

    // The v8 isolate associated to the (worker or worklet) global scope. For
    // workers this should be the worker thread's isolate, while for worklets
    // usually the main thread's isolate is used.
    v8::Isolate* m_isolate;

    RefPtr<ScriptState> m_scriptState;
    RefPtr<DOMWrapperWorld> m_world;
    String m_disableEvalPending;
    bool m_executionForbidden;
    bool m_executionScheduledToTerminate;
    mutable Mutex m_scheduledTerminationMutex;

    RefPtr<RejectedPromises> m_rejectedPromises;

    // |m_executionState| refers to a stack object that evaluate() allocates;
    // evaluate() ensuring that the pointer reference to it is removed upon
    // returning. Hence kept as a bare pointer here, and not a Persistent with
    // Oilpan enabled; stack scanning will visit the object and
    // trace its on-heap fields.
    GC_PLUGIN_IGNORE("394615")
    ExecutionState* m_executionState;
};

} // namespace blink

#endif // WorkerOrWorkletScriptController_h
