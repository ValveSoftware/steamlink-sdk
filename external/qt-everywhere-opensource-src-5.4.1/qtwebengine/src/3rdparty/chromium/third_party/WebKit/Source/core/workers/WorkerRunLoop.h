/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WorkerRunLoop_h
#define WorkerRunLoop_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/MessageQueue.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

    class WorkerGlobalScope;
    class WorkerSharedTimer;

    class WorkerRunLoop {
    public:
        WorkerRunLoop();
        ~WorkerRunLoop();

        void setWorkerGlobalScope(WorkerGlobalScope*);

        // Blocking call. Waits for tasks and timers, invokes the callbacks.
        void run();

        enum WaitMode { WaitForMessage, DontWaitForMessage };

        // Waits for a single debugger task and returns.
        MessageQueueWaitResult runDebuggerTask(WaitMode = WaitForMessage);

        void terminate();
        bool terminated() const { return m_messageQueue.killed(); }

        WorkerGlobalScope* context() const { return m_context; }

        // Returns true if the loop is still alive, false if it has been terminated.
        bool postTask(PassOwnPtr<ExecutionContextTask>);

        void postTaskAndTerminate(PassOwnPtr<ExecutionContextTask>);

        // Returns true if the loop is still alive, false if it has been terminated.
        bool postDebuggerTask(PassOwnPtr<ExecutionContextTask>);

    private:
        friend class RunLoopSetup;
        MessageQueueWaitResult run(MessageQueue<blink::WebThread::Task>&, WaitMode);

        // Runs any clean up tasks that are currently in the queue and returns.
        // This should only be called when the context is closed or loop has been terminated.
        void runCleanupTasks();

        MessageQueue<blink::WebThread::Task> m_messageQueue;
        MessageQueue<blink::WebThread::Task> m_debuggerMessageQueue;
        OwnPtr<WorkerSharedTimer> m_sharedTimer;
        WorkerGlobalScope* m_context;
        int m_nestedCount;
    };

} // namespace WebCore

#endif // WorkerRunLoop_h
