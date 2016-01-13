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

#ifndef WorkerScriptController_h
#define WorkerScriptController_h

#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "core/events/ErrorEvent.h"
#include "wtf/OwnPtr.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/TextPosition.h"
#include <v8.h>

namespace WebCore {

    class ScriptSourceCode;
    class ScriptValue;
    class WorkerGlobalScope;

    struct WorkerGlobalScopeExecutionState {
        WorkerGlobalScopeExecutionState()
            : hadException(false)
            , lineNumber(0)
            , columnNumber(0)
        {
        }

        bool hadException;
        String errorMessage;
        int lineNumber;
        int columnNumber;
        String sourceURL;
        ScriptValue exception;
    };

    class WorkerScriptController {
    public:
        explicit WorkerScriptController(WorkerGlobalScope&);
        ~WorkerScriptController();

        WorkerGlobalScope& workerGlobalScope() { return m_workerGlobalScope; }

        bool initializeContextIfNeeded();

        void evaluate(const ScriptSourceCode&, RefPtrWillBeRawPtr<ErrorEvent>* = 0);

        void rethrowExceptionFromImportedScript(PassRefPtrWillBeRawPtr<ErrorEvent>);

        // Async request to terminate a future JS execution. Eventually causes termination
        // exception raised during JS execution, if the worker thread happens to run JS.
        // After JS execution was terminated in this way, the Worker thread has to use
        // forbidExecution()/isExecutionForbidden() to guard against reentry into JS.
        // Can be called from any thread.
        void scheduleExecutionTermination();
        bool isExecutionTerminating() const;

        // Called on Worker thread when JS exits with termination exception caused by forbidExecution() request,
        // or by Worker thread termination code to prevent future entry into JS.
        void forbidExecution();
        bool isExecutionForbidden() const;

        void disableEval(const String&);

        // Evaluate a script file in the current execution environment.
        ScriptValue evaluate(const String& script, const String& fileName, const TextPosition& scriptStartPosition, WorkerGlobalScopeExecutionState*);

        v8::Isolate* isolate() const { return m_isolate; }
        DOMWrapperWorld& world() const { return *m_world; }
        ScriptState* scriptState() { return m_scriptState.get(); }
        v8::Local<v8::Context> context() { return m_scriptState ? m_scriptState->context() : v8::Local<v8::Context>(); }
        bool isContextInitialized() { return m_scriptState && !!m_scriptState->perContextData(); }

        // Send a notification about current thread is going to be idle.
        // Returns true if the embedder should stop calling idleNotification
        // until real work has been done.
        bool idleNotification() { return v8::V8::IdleNotification(); }


    private:
        v8::Isolate* m_isolate;
        WorkerGlobalScope& m_workerGlobalScope;
        RefPtr<ScriptState> m_scriptState;
        RefPtr<DOMWrapperWorld> m_world;
        String m_disableEvalPending;
        bool m_executionForbidden;
        bool m_executionScheduledToTerminate;
        mutable Mutex m_scheduledTerminationMutex;
        RefPtrWillBePersistent<ErrorEvent> m_errorEventFromImportedScript;
        OwnPtr<V8IsolateInterruptor> m_interruptor;
    };

} // namespace WebCore

#endif // WorkerScriptController_h
