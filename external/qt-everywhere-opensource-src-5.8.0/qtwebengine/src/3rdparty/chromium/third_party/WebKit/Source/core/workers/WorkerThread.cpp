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

#include "core/workers/WorkerThread.h"

#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IdleTaskRunner.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "platform/scheduler/CancellableTaskFactory.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/Threading.h"
#include "wtf/text/WTFString.h"
#include <limits.h>
#include <memory>

namespace blink {

// TODO(nhiroki): Adjust the delay based on UMA.
const long long kForceTerminationDelayInMs = 2000; // 2 secs

// ForceTerminationTask is used for posting a delayed task to terminate the
// worker execution from the main thread. This task is expected to run when the
// shutdown sequence does not start in a certain time period because of an
// inifite loop in the JS execution context etc. When the shutdown sequence is
// started before this task runs, the task is simply cancelled.
class WorkerThread::ForceTerminationTask final {
public:
    static std::unique_ptr<ForceTerminationTask> create(WorkerThread* workerThread)
    {
        return wrapUnique(new ForceTerminationTask(workerThread));
    }

    void schedule()
    {
        DCHECK(isMainThread());
        Platform::current()->mainThread()->getWebTaskRunner()->postDelayedTask(BLINK_FROM_HERE, m_cancellableTaskFactory->cancelAndCreate(), m_workerThread->m_forceTerminationDelayInMs);
    }

private:
    explicit ForceTerminationTask(WorkerThread* workerThread)
        : m_workerThread(workerThread)
    {
        DCHECK(isMainThread());
        m_cancellableTaskFactory = CancellableTaskFactory::create(this, &ForceTerminationTask::run);
    }

    void run()
    {
        DCHECK(isMainThread());
        MutexLocker lock(m_workerThread->m_threadStateMutex);
        if (m_workerThread->m_readyToShutdown) {
            // Shutdown sequence is now running. Just return.
            return;
        }
        if (m_workerThread->m_runningDebuggerTask) {
            // Any debugger task is guaranteed to finish, so we can wait for the
            // completion. Shutdown sequence will start after that.
            return;
        }

        m_workerThread->forciblyTerminateExecution();
        DCHECK_EQ(WorkerThread::ExitCode::NotTerminated, m_workerThread->m_exitCode);
        m_workerThread->m_exitCode = WorkerThread::ExitCode::AsyncForciblyTerminated;
    }

    WorkerThread* m_workerThread;
    std::unique_ptr<CancellableTaskFactory> m_cancellableTaskFactory;
};

class WorkerThread::WorkerMicrotaskRunner final : public WebThread::TaskObserver {
public:
    explicit WorkerMicrotaskRunner(WorkerThread* workerThread)
        : m_workerThread(workerThread)
    {
    }

    void willProcessTask() override
    {
        // No tasks should get executed after we have closed.
        DCHECK(!m_workerThread->workerGlobalScope() || !m_workerThread->workerGlobalScope()->isClosing());
    }

    void didProcessTask() override
    {
        Microtask::performCheckpoint(m_workerThread->isolate());
        if (WorkerGlobalScope* globalScope = m_workerThread->workerGlobalScope()) {
            if (WorkerOrWorkletScriptController* scriptController = globalScope->scriptController())
                scriptController->getRejectedPromises()->processQueue();
            if (globalScope->isClosing()) {
                // |m_workerThread| will eventually be requested to terminate.
                m_workerThread->workerReportingProxy().workerGlobalScopeClosed();

                // Stop further worker tasks to run after this point.
                m_workerThread->prepareForShutdownOnWorkerThread();
            }
        }
    }

private:
    // Thread owns the microtask runner; reference remains
    // valid for the lifetime of this object.
    WorkerThread* m_workerThread;
};

static Mutex& threadSetMutex()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, new Mutex);
    return mutex;
}

static HashSet<WorkerThread*>& workerThreads()
{
    DEFINE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
    return threads;
}

WorkerThreadLifecycleContext::WorkerThreadLifecycleContext()
{
    DCHECK(isMainThread());
}

WorkerThreadLifecycleContext::~WorkerThreadLifecycleContext()
{
    DCHECK(isMainThread());
}

void WorkerThreadLifecycleContext::notifyContextDestroyed()
{
    DCHECK(isMainThread());
    DCHECK(!m_wasContextDestroyed);
    m_wasContextDestroyed = true;
    LifecycleNotifier::notifyContextDestroyed();
}

WorkerThread::~WorkerThread()
{
    DCHECK(isMainThread());
    MutexLocker lock(threadSetMutex());
    DCHECK(workerThreads().contains(this));
    workerThreads().remove(this);

    DCHECK_NE(ExitCode::NotTerminated, m_exitCode);
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, exitCodeHistogram, new EnumerationHistogram("WorkerThread.ExitCode", static_cast<int>(ExitCode::LastEnum)));
    exitCodeHistogram.count(static_cast<int>(m_exitCode));
}

void WorkerThread::start(std::unique_ptr<WorkerThreadStartupData> startupData)
{
    DCHECK(isMainThread());

    if (m_started)
        return;

    m_started = true;
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&WorkerThread::initializeOnWorkerThread, crossThreadUnretained(this), passed(std::move(startupData))));
}

void WorkerThread::terminate()
{
    DCHECK(isMainThread());
    terminateInternal(TerminationMode::Graceful);
}

void WorkerThread::terminateAndWait()
{
    DCHECK(isMainThread());

    // The main thread will be blocked, so asynchronous graceful shutdown does
    // not work.
    terminateInternal(TerminationMode::Forcible);
    m_shutdownEvent->wait();
}

void WorkerThread::terminateAndWaitForAllWorkers()
{
    DCHECK(isMainThread());

    // Keep this lock to prevent WorkerThread instances from being destroyed.
    MutexLocker lock(threadSetMutex());
    HashSet<WorkerThread*> threads = workerThreads();

    // The main thread will be blocked, so asynchronous graceful shutdown does
    // not work.
    for (WorkerThread* thread : threads)
        thread->terminateInternal(TerminationMode::Forcible);

    for (WorkerThread* thread : threads)
        thread->m_shutdownEvent->wait();
}

v8::Isolate* WorkerThread::isolate()
{
    return workerBackingThread().isolate();
}

bool WorkerThread::isCurrentThread()
{
    return workerBackingThread().backingThread().isCurrentThread();
}

void WorkerThread::postTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task, bool isInstrumented)
{
    if (isInShutdown())
        return;
    if (isInstrumented) {
        DCHECK(isCurrentThread());
        InspectorInstrumentation::asyncTaskScheduled(workerGlobalScope(), "Worker task", task.get());
    }
    workerBackingThread().backingThread().postTask(location, crossThreadBind(&WorkerThread::performTaskOnWorkerThread, crossThreadUnretained(this), passed(std::move(task)), isInstrumented));
}

void WorkerThread::appendDebuggerTask(std::unique_ptr<CrossThreadClosure> task)
{
    DCHECK(isMainThread());
    if (isInShutdown())
        return;
    m_inspectorTaskRunner->appendTask(crossThreadBind(&WorkerThread::performDebuggerTaskOnWorkerThread, crossThreadUnretained(this), passed(std::move(task))));
    {
        MutexLocker lock(m_threadStateMutex);
        if (isolate() && !m_readyToShutdown)
            m_inspectorTaskRunner->interruptAndRunAllTasksDontWait(isolate());
    }
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&WorkerThread::performDebuggerTaskDontWaitOnWorkerThread, crossThreadUnretained(this)));
}

void WorkerThread::startRunningDebuggerTasksOnPauseOnWorkerThread()
{
    DCHECK(isCurrentThread());
    m_pausedInDebugger = true;
    ThreadDebugger::idleStarted(isolate());
    std::unique_ptr<CrossThreadClosure> task;
    do {
        {
            SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);
            task = m_inspectorTaskRunner->takeNextTask(InspectorTaskRunner::WaitForTask);
        }
        if (task)
            (*task)();
    // Keep waiting until execution is resumed.
    } while (task && m_pausedInDebugger);
    ThreadDebugger::idleFinished(isolate());
}

void WorkerThread::stopRunningDebuggerTasksOnPauseOnWorkerThread()
{
    DCHECK(isCurrentThread());
    m_pausedInDebugger = false;
}

WorkerGlobalScope* WorkerThread::workerGlobalScope()
{
    DCHECK(isCurrentThread());
    return m_workerGlobalScope.get();
}

bool WorkerThread::terminated()
{
    MutexLocker lock(m_threadStateMutex);
    return m_terminated;
}

unsigned WorkerThread::workerThreadCount()
{
    MutexLocker lock(threadSetMutex());
    return workerThreads().size();
}

PlatformThreadId WorkerThread::platformThreadId()
{
    if (!m_started)
        return 0;
    return workerBackingThread().backingThread().platformThread().threadId();
}

WorkerThread::WorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerReportingProxy& workerReportingProxy)
    : m_forceTerminationDelayInMs(kForceTerminationDelayInMs)
    , m_inspectorTaskRunner(wrapUnique(new InspectorTaskRunner()))
    , m_workerLoaderProxy(workerLoaderProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_terminationEvent(wrapUnique(new WaitableEvent(
        WaitableEvent::ResetPolicy::Manual,
        WaitableEvent::InitialState::NonSignaled)))
    , m_shutdownEvent(wrapUnique(new WaitableEvent(
        WaitableEvent::ResetPolicy::Manual,
        WaitableEvent::InitialState::NonSignaled)))
    , m_workerThreadLifecycleContext(new WorkerThreadLifecycleContext)
{
    DCHECK(isMainThread());
    MutexLocker lock(threadSetMutex());
    workerThreads().add(this);
}

void WorkerThread::terminateInternal(TerminationMode mode)
{
    DCHECK(isMainThread());
    DCHECK(m_started);

    // Prevent the deadlock between GC and an attempt to terminate a thread.
    SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);

    // Protect against this method, initializeOnWorkerThread() or termination
    // via the global scope racing each other.
    MutexLocker lock(m_threadStateMutex);

    // If terminate has already been called.
    if (m_terminated) {
        if (m_runningDebuggerTask) {
            // Any debugger task is guaranteed to finish, so we can wait for the
            // completion even if the synchronous forcible termination is
            // requested. Shutdown sequence will start after the task.
            DCHECK(!m_scheduledForceTerminationTask);
            return;
        }

        // The synchronous forcible termination request should overtake the
        // scheduled termination task because the request will block the main
        // thread and the scheduled termination task never runs.
        if (mode == TerminationMode::Forcible && m_exitCode == ExitCode::NotTerminated) {
            DCHECK(m_scheduledForceTerminationTask);
            m_scheduledForceTerminationTask.reset();
            forciblyTerminateExecution();
            DCHECK_EQ(ExitCode::NotTerminated, m_exitCode);
            m_exitCode = ExitCode::SyncForciblyTerminated;
        }
        return;
    }
    m_terminated = true;

    // Signal the thread to notify that the thread's stopping.
    if (m_terminationEvent)
        m_terminationEvent->signal();

    m_workerThreadLifecycleContext->notifyContextDestroyed();

    // If the worker thread was never initialized, don't start another
    // shutdown, but still wait for the thread to signal when shutdown has
    // completed on initializeOnWorkerThread().
    if (!m_workerGlobalScope) {
        DCHECK_EQ(ExitCode::NotTerminated, m_exitCode);
        m_exitCode = ExitCode::GracefullyTerminated;
        return;
    }

    // Determine if we should synchronously terminate or schedule to terminate
    // the worker execution so that the task can be handled by thread event
    // loop. If script execution weren't forbidden, a while(1) loop in JS could
    // keep the thread alive forever.
    //
    // (1) |m_readyToShutdown|: If this is set, the worker thread has already
    // noticed that the thread is about to be terminated and the worker global
    // scope is already disposed, so we don't have to explicitly terminate the
    // worker execution.
    //
    // (2) |m_runningDebuggerTask|: Terminating during debugger task may lead to
    // crash due to heavy use of v8 api in debugger. Any debugger task is
    // guaranteed to finish, so we can wait for the completion.
    bool shouldScheduleToTerminateExecution = !m_readyToShutdown && !m_runningDebuggerTask;

    if (shouldScheduleToTerminateExecution) {
        if (mode == TerminationMode::Forcible) {
            forciblyTerminateExecution();
            DCHECK_EQ(ExitCode::NotTerminated, m_exitCode);
            m_exitCode = ExitCode::SyncForciblyTerminated;
        } else {
            DCHECK_EQ(TerminationMode::Graceful, mode);
            DCHECK(!m_scheduledForceTerminationTask);
            m_scheduledForceTerminationTask = ForceTerminationTask::create(this);
            m_scheduledForceTerminationTask->schedule();
        }
    }

    m_inspectorTaskRunner->kill();
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&WorkerThread::prepareForShutdownOnWorkerThread, crossThreadUnretained(this)));
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&WorkerThread::performShutdownOnWorkerThread, crossThreadUnretained(this)));
}

void WorkerThread::forciblyTerminateExecution()
{
    DCHECK(m_workerGlobalScope);
    m_workerGlobalScope->scriptController()->willScheduleExecutionTermination();
    isolate()->TerminateExecution();
}

bool WorkerThread::isInShutdown()
{
    // Check if we've started termination or shutdown sequence. Avoid acquiring
    // a lock here to avoid introducing a risk of deadlock. Note that accessing
    // |m_terminated| on the main thread or |m_readyToShutdown| on the worker
    // thread is safe as the flag is set only on the thread.
    if (isMainThread() && m_terminated)
        return true;
    if (isCurrentThread() && m_readyToShutdown)
        return true;
    return false;
}

void WorkerThread::initializeOnWorkerThread(std::unique_ptr<WorkerThreadStartupData> startupData)
{
    DCHECK(isCurrentThread());
    KURL scriptURL = startupData->m_scriptURL;
    String sourceCode = startupData->m_sourceCode;
    WorkerThreadStartMode startMode = startupData->m_startMode;
    std::unique_ptr<Vector<char>> cachedMetaData = std::move(startupData->m_cachedMetaData);
    V8CacheOptions v8CacheOptions = startupData->m_v8CacheOptions;

    {
        MutexLocker lock(m_threadStateMutex);

        // The worker was terminated before the thread had a chance to run.
        if (m_terminated) {
            DCHECK_EQ(ExitCode::GracefullyTerminated, m_exitCode);

            // Notify the proxy that the WorkerGlobalScope has been disposed of.
            // This can free this thread object, hence it must not be touched
            // afterwards.
            m_workerReportingProxy.workerThreadTerminated();

            // Notify the main thread that it is safe to deallocate our
            // resources.
            m_shutdownEvent->signal();
            return;
        }

        if (isOwningBackingThread())
            workerBackingThread().initialize();

        if (shouldAttachThreadDebugger())
            V8PerIsolateData::from(isolate())->setThreadDebugger(wrapUnique(new WorkerThreadDebugger(this, isolate())));
        m_microtaskRunner = wrapUnique(new WorkerMicrotaskRunner(this));
        workerBackingThread().backingThread().addTaskObserver(m_microtaskRunner.get());

        // Optimize for memory usage instead of latency for the worker isolate.
        isolate()->IsolateInBackgroundNotification();
        m_workerGlobalScope = createWorkerGlobalScope(std::move(startupData));
        m_workerGlobalScope->scriptLoaded(sourceCode.length(), cachedMetaData.get() ? cachedMetaData->size() : 0);

        // Notify proxy that a new WorkerGlobalScope has been created and started.
        m_workerReportingProxy.workerGlobalScopeStarted(m_workerGlobalScope.get());

        WorkerOrWorkletScriptController* scriptController = m_workerGlobalScope->scriptController();
        if (!scriptController->isExecutionForbidden()) {
            scriptController->initializeContextIfNeeded();

            // If Origin Trials have been registered before the V8 context was ready,
            // then inject them into the context now
            ExecutionContext* executionContext = m_workerGlobalScope->getExecutionContext();
            if (executionContext) {
                OriginTrialContext* originTrialContext = OriginTrialContext::from(executionContext);
                if (originTrialContext)
                    originTrialContext->initializePendingFeatures();
            }
        }
    }

    if (startMode == PauseWorkerGlobalScopeOnStart) {
        startRunningDebuggerTasksOnPauseOnWorkerThread();

        // WorkerThread may be ready to shut down at this point if termination
        // is requested while the debugger task is running. Shutdown sequence
        // will start soon.
        if (m_readyToShutdown)
            return;
    }

    if (m_workerGlobalScope->scriptController()->isContextInitialized()) {
        m_workerReportingProxy.didInitializeWorkerContext();
        v8::HandleScope handleScope(isolate());
        Platform::current()->workerContextCreated(m_workerGlobalScope->scriptController()->context());
    }

    CachedMetadataHandler* handler = workerGlobalScope()->createWorkerScriptCachedMetadataHandler(scriptURL, cachedMetaData.get());
    bool success = m_workerGlobalScope->scriptController()->evaluate(ScriptSourceCode(sourceCode, scriptURL), nullptr, handler, v8CacheOptions);
    m_workerGlobalScope->didEvaluateWorkerScript();
    m_workerReportingProxy.didEvaluateWorkerScript(success);

    postInitialize();
}

void WorkerThread::prepareForShutdownOnWorkerThread()
{
    DCHECK(isCurrentThread());
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_readyToShutdown)
            return;
        m_readyToShutdown = true;
        if (m_exitCode == ExitCode::NotTerminated)
            m_exitCode = ExitCode::GracefullyTerminated;
    }

    m_inspectorTaskRunner->kill();
    workerReportingProxy().willDestroyWorkerGlobalScope();
    InspectorInstrumentation::allAsyncTasksCanceled(workerGlobalScope());
    workerGlobalScope()->dispose();
    workerBackingThread().backingThread().removeTaskObserver(m_microtaskRunner.get());
}

void WorkerThread::performShutdownOnWorkerThread()
{
    DCHECK(isCurrentThread());
#if DCHECK_IS_ON
    {
        MutexLocker lock(m_threadStateMutex);
        DCHECK(m_terminated);
        DCHECK(m_readyToShutdown);
    }
#endif

    // The below assignment will destroy the context, which will in turn notify
    // messaging proxy. We cannot let any objects survive past thread exit,
    // because no other thread will run GC or otherwise destroy them. If Oilpan
    // is enabled, we detach of the context/global scope, with the final heap
    // cleanup below sweeping it out.
    m_workerGlobalScope->notifyContextDestroyed();
    m_workerGlobalScope = nullptr;

    if (isOwningBackingThread())
        workerBackingThread().shutdown();
    // We must not touch workerBackingThread() from now on.

    m_microtaskRunner = nullptr;

    // Notify the proxy that the WorkerGlobalScope has been disposed of.
    // This can free this thread object, hence it must not be touched
    // afterwards.
    workerReportingProxy().workerThreadTerminated();

    m_shutdownEvent->signal();
}

void WorkerThread::performTaskOnWorkerThread(std::unique_ptr<ExecutionContextTask> task, bool isInstrumented)
{
    DCHECK(isCurrentThread());
    if (isInShutdown())
        return;

    WorkerGlobalScope* globalScope = workerGlobalScope();
    // If the thread is terminated before it had a chance initialize (see
    // WorkerThread::Initialize()), we mustn't run any of the posted tasks.
    if (!globalScope) {
        DCHECK(terminated());
        return;
    }

    InspectorInstrumentation::AsyncTask asyncTask(globalScope, task.get(), isInstrumented);
    {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounter, new CustomCountHistogram("WorkerThread.Task.Time", 0, 10000000, 50));
        ScopedUsHistogramTimer timer(scopedUsCounter);
        task->performTask(globalScope);
    }
}

void WorkerThread::performDebuggerTaskOnWorkerThread(std::unique_ptr<CrossThreadClosure> task)
{
    DCHECK(isCurrentThread());
    InspectorTaskRunner::IgnoreInterruptsScope scope(m_inspectorTaskRunner.get());
    {
        MutexLocker lock(m_threadStateMutex);
        DCHECK(!m_readyToShutdown);
        m_runningDebuggerTask = true;
    }
    ThreadDebugger::idleFinished(isolate());
    {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounter, new CustomCountHistogram("WorkerThread.DebuggerTask.Time", 0, 10000000, 50));
        ScopedUsHistogramTimer timer(scopedUsCounter);
        (*task)();
    }
    ThreadDebugger::idleStarted(isolate());
    {
        MutexLocker lock(m_threadStateMutex);
        if (!m_terminated) {
            m_runningDebuggerTask = false;
            return;
        }
        // terminate() was called. Shutdown sequence will start soon.
        // Keep |m_runningDebuggerTask| to prevent forcible termination from the
        // main thread before shutdown preparation.
    }
    // Stop further worker tasks to run after this point.
    prepareForShutdownOnWorkerThread();
}

void WorkerThread::performDebuggerTaskDontWaitOnWorkerThread()
{
    DCHECK(isCurrentThread());
    std::unique_ptr<CrossThreadClosure> task = m_inspectorTaskRunner->takeNextTask(InspectorTaskRunner::DontWaitForTask);
    if (task)
        (*task)();
}

WorkerThread::ExitCode WorkerThread::getExitCode()
{
    MutexLocker lock(m_threadStateMutex);
    return m_exitCode;
}

} // namespace blink
