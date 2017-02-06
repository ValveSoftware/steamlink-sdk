/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerThread_h
#define WorkerThread_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "platform/LifecycleNotifier.h"
#include "platform/WaitableEvent.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"
#include "wtf/PassRefPtr.h"
#include <memory>
#include <v8.h>

namespace blink {

class InspectorTaskRunner;
class WorkerBackingThread;
class WorkerGlobalScope;
class WorkerInspectorController;
class WorkerReportingProxy;
class WorkerThreadStartupData;

enum WorkerThreadStartMode {
    DontPauseWorkerGlobalScopeOnStart,
    PauseWorkerGlobalScopeOnStart
};

// Used for notifying observers on the main thread of worker thread termination.
// The lifetime of this class is equal to that of WorkerThread. Created and
// destructed on the main thread.
class CORE_EXPORT WorkerThreadLifecycleContext final : public GarbageCollectedFinalized<WorkerThreadLifecycleContext>, public LifecycleNotifier<WorkerThreadLifecycleContext, WorkerThreadLifecycleObserver> {
    USING_GARBAGE_COLLECTED_MIXIN(WorkerThreadLifecycleContext);
    WTF_MAKE_NONCOPYABLE(WorkerThreadLifecycleContext);
public:
    WorkerThreadLifecycleContext();
    ~WorkerThreadLifecycleContext() override;
    void notifyContextDestroyed() override;

private:
    friend class WorkerThreadLifecycleObserver;
    bool m_wasContextDestroyed = false;
};

// WorkerThread is a kind of WorkerBackingThread client. Each worker mechanism
// can access the lower thread infrastructure via an implementation of this
// abstract class. Multiple WorkerThreads can share one WorkerBackingThread.
// See WorkerBackingThread.h for more details.
//
// WorkerThread start and termination must be initiated on the main thread and
// an actual task is executed on the worker thread.
//
// When termination starts, (debugger) tasks on WorkerThread are handled as
// follows:
//  - A running task may finish unless a forcible termination task interrupts.
//    If the running task is for debugger, it's guaranteed to finish without
//    any interruptions.
//  - Queued tasks never run.
//  - postTask() and appendDebuggerTask() reject posting new tasks.
class CORE_EXPORT WorkerThread {
public:
    // Represents how this thread is terminated. Used for UMA. Append only.
    enum class ExitCode {
        NotTerminated,
        GracefullyTerminated,
        SyncForciblyTerminated,
        AsyncForciblyTerminated,
        LastEnum,
    };

    virtual ~WorkerThread();

    // Called on the main thread.
    void start(std::unique_ptr<WorkerThreadStartupData>);
    void terminate();

    // Called on the main thread. Internally calls terminateInternal() and wait
    // (by *blocking* the calling thread) until the worker(s) is/are shut down.
    void terminateAndWait();
    static void terminateAndWaitForAllWorkers();

    virtual WorkerBackingThread& workerBackingThread() = 0;
    virtual bool shouldAttachThreadDebugger() const { return true; }
    v8::Isolate* isolate();

    // Can be used to wait for this worker thread to terminate.
    // (This is signaled on the main thread, so it's assumed to be waited on
    // the worker context thread)
    WaitableEvent* terminationEvent() { return m_terminationEvent.get(); }

    bool isCurrentThread();

    WorkerLoaderProxy* workerLoaderProxy() const
    {
        RELEASE_ASSERT(m_workerLoaderProxy);
        return m_workerLoaderProxy.get();
    }

    WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

    void postTask(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>, bool isInstrumented = false);
    void appendDebuggerTask(std::unique_ptr<CrossThreadClosure>);

    // Runs only debugger tasks while paused in debugger.
    void startRunningDebuggerTasksOnPauseOnWorkerThread();
    void stopRunningDebuggerTasksOnPauseOnWorkerThread();

    // Can be called only on the worker thread, WorkerGlobalScope is not thread
    // safe.
    WorkerGlobalScope* workerGlobalScope();

    // Called for creating WorkerThreadLifecycleObserver on both the main thread
    // and the worker thread.
    WorkerThreadLifecycleContext* getWorkerThreadLifecycleContext() const { return m_workerThreadLifecycleContext; }

    // Returns true once one of the terminate* methods is called.
    bool terminated();

    // Number of active worker threads.
    static unsigned workerThreadCount();

    PlatformThreadId platformThreadId();

    ExitCode getExitCode();

    void waitForShutdownForTesting() { m_shutdownEvent->wait(); }

protected:
    WorkerThread(PassRefPtr<WorkerLoaderProxy>, WorkerReportingProxy&);

    // Factory method for creating a new worker context for the thread.
    // Called on the worker thread.
    virtual WorkerGlobalScope* createWorkerGlobalScope(std::unique_ptr<WorkerThreadStartupData>) = 0;

    // Returns true when this WorkerThread owns the associated
    // WorkerBackingThread exclusively. If this function returns true, the
    // WorkerThread initializes / shutdowns the backing thread. Otherwise
    // workerBackingThread() should be initialized / shutdown properly
    // out of this class.
    virtual bool isOwningBackingThread() const { return true; }

    // Called on the worker thread.
    virtual void postInitialize() { }

private:
    friend class WorkerThreadTest;
    FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest, StartAndTerminateOnInitialization_TerminateWhileDebuggerTaskIsRunning);
    FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest, StartAndTerminateOnScriptLoaded_TerminateWhileDebuggerTaskIsRunning);

    class ForceTerminationTask;
    class WorkerMicrotaskRunner;

    enum class TerminationMode {
        // Synchronously terminate the worker execution. Please be careful to
        // use this mode, because after the synchronous termination any V8 APIs
        // may suddenly start to return empty handles and it may cause crashes.
        Forcible,

        // Don't synchronously terminate the worker execution. Instead, schedule
        // a task to terminate it in case that the shutdown sequence does not
        // start on the worker thread in a certain time period.
        Graceful,
    };

    void terminateInternal(TerminationMode);
    void forciblyTerminateExecution();

    // Returns true if termination or shutdown sequence has started. This is
    // thread safe.
    // Note that this returns false when the sequence has already started but it
    // hasn't been notified to the calling thread.
    bool isInShutdown();

    void initializeOnWorkerThread(std::unique_ptr<WorkerThreadStartupData>);
    void prepareForShutdownOnWorkerThread();
    void performShutdownOnWorkerThread();
    void performTaskOnWorkerThread(std::unique_ptr<ExecutionContextTask>, bool isInstrumented);
    void performDebuggerTaskOnWorkerThread(std::unique_ptr<CrossThreadClosure>);
    void performDebuggerTaskDontWaitOnWorkerThread();

    // Accessed only on the main thread.
    bool m_started = false;

    // Set on the main thread and checked on both the main and worker threads.
    bool m_terminated = false;

    // Set on the worker thread and checked on both the main and worker threads.
    bool m_readyToShutdown = false;

    // Accessed only on the worker thread.
    bool m_pausedInDebugger = false;

    // Set on the worker thread and checked on both the main and worker threads.
    bool m_runningDebuggerTask = false;

    ExitCode m_exitCode = ExitCode::NotTerminated;

    long long m_forceTerminationDelayInMs;

    std::unique_ptr<InspectorTaskRunner> m_inspectorTaskRunner;
    std::unique_ptr<WorkerMicrotaskRunner> m_microtaskRunner;

    RefPtr<WorkerLoaderProxy> m_workerLoaderProxy;
    WorkerReportingProxy& m_workerReportingProxy;

    // This lock protects |m_workerGlobalScope|, |m_terminated|,
    // |m_readyToShutdown|, |m_runningDebuggerTask|, |m_exitCode| and
    // |m_microtaskRunner|.
    Mutex m_threadStateMutex;

    Persistent<WorkerGlobalScope> m_workerGlobalScope;

    // Signaled when the thread starts termination on the main thread.
    std::unique_ptr<WaitableEvent> m_terminationEvent;

    // Signaled when the thread completes termination on the worker thread.
    std::unique_ptr<WaitableEvent> m_shutdownEvent;

    // Scheduled when termination starts with TerminationMode::Force, and
    // cancelled when the worker thread is gracefully shut down.
    std::unique_ptr<ForceTerminationTask> m_scheduledForceTerminationTask;

    Persistent<WorkerThreadLifecycleContext> m_workerThreadLifecycleContext;
};

} // namespace blink

#endif // WorkerThread_h
