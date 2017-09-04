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
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/Threading.h"
#include "wtf/text/WTFString.h"
#include <limits.h>
#include <memory>

namespace blink {

using ExitCode = WorkerThread::ExitCode;

// TODO(nhiroki): Adjust the delay based on UMA.
const long long kForcibleTerminationDelayInMs = 2000;  // 2 secs

static Mutex& threadSetMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, new Mutex);
  return mutex;
}

static int getNextWorkerThreadId() {
  DCHECK(isMainThread());
  static int nextWorkerThreadId = 1;
  CHECK_LT(nextWorkerThreadId, std::numeric_limits<int>::max());
  return nextWorkerThreadId++;
}

WorkerThreadLifecycleContext::WorkerThreadLifecycleContext() {
  DCHECK(isMainThread());
}

WorkerThreadLifecycleContext::~WorkerThreadLifecycleContext() {
  DCHECK(isMainThread());
}

void WorkerThreadLifecycleContext::notifyContextDestroyed() {
  DCHECK(isMainThread());
  DCHECK(!m_wasContextDestroyed);
  m_wasContextDestroyed = true;
  LifecycleNotifier::notifyContextDestroyed();
}

WorkerThread::~WorkerThread() {
  DCHECK(isMainThread());
  MutexLocker lock(threadSetMutex());
  DCHECK(workerThreads().contains(this));
  workerThreads().remove(this);

  DCHECK_NE(ExitCode::NotTerminated, m_exitCode);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, exitCodeHistogram,
      new EnumerationHistogram("WorkerThread.ExitCode",
                               static_cast<int>(ExitCode::LastEnum)));
  exitCodeHistogram.count(static_cast<int>(m_exitCode));
}

void WorkerThread::start(std::unique_ptr<WorkerThreadStartupData> startupData) {
  DCHECK(isMainThread());

  if (m_requestedToStart)
    return;

  m_requestedToStart = true;
  workerBackingThread().backingThread().postTask(
      BLINK_FROM_HERE, crossThreadBind(&WorkerThread::initializeOnWorkerThread,
                                       crossThreadUnretained(this),
                                       passed(std::move(startupData))));
}

void WorkerThread::terminate() {
  DCHECK(isMainThread());
  terminateInternal(TerminationMode::Graceful);
}

void WorkerThread::terminateAndWait() {
  DCHECK(isMainThread());

  // The main thread will be blocked, so asynchronous graceful shutdown does
  // not work.
  terminateInternal(TerminationMode::Forcible);
  m_shutdownEvent->wait();

  // Destruct base::Thread and join the underlying system thread.
  clearWorkerBackingThread();
}

void WorkerThread::terminateAndWaitForAllWorkers() {
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

  // Destruct base::Thread and join the underlying system threads.
  for (WorkerThread* thread : threads)
    thread->clearWorkerBackingThread();
}

void WorkerThread::willProcessTask() {
  DCHECK(isCurrentThread());

  // No tasks should get executed after we have closed.
  DCHECK(!globalScope()->isClosing());

  if (isForciblyTerminated()) {
    // The script has been terminated forcibly, which means we need to
    // ask objects in the thread to stop working as soon as possible.
    prepareForShutdownOnWorkerThread();
  }
}

void WorkerThread::didProcessTask() {
  DCHECK(isCurrentThread());
  Microtask::performCheckpoint(isolate());
  globalScope()->scriptController()->getRejectedPromises()->processQueue();
  if (globalScope()->isClosing()) {
    // This WorkerThread will eventually be requested to terminate.
    workerReportingProxy().didCloseWorkerGlobalScope();

    // Stop further worker tasks to run after this point.
    prepareForShutdownOnWorkerThread();
  }
}

v8::Isolate* WorkerThread::isolate() {
  return workerBackingThread().isolate();
}

bool WorkerThread::isCurrentThread() {
  return workerBackingThread().backingThread().isCurrentThread();
}

void WorkerThread::postTask(const WebTraceLocation& location,
                            std::unique_ptr<ExecutionContextTask> task,
                            bool isInstrumented) {
  if (isInShutdown())
    return;
  if (isInstrumented) {
    DCHECK(isCurrentThread());
    InspectorInstrumentation::asyncTaskScheduled(globalScope(), "Worker task",
                                                 task.get());
  }
  workerBackingThread().backingThread().postTask(
      location, crossThreadBind(&WorkerThread::performTaskOnWorkerThread,
                                crossThreadUnretained(this),
                                passed(std::move(task)), isInstrumented));
}

void WorkerThread::appendDebuggerTask(
    std::unique_ptr<CrossThreadClosure> task) {
  DCHECK(isMainThread());
  if (isInShutdown())
    return;
  m_inspectorTaskRunner->appendTask(
      crossThreadBind(&WorkerThread::performDebuggerTaskOnWorkerThread,
                      crossThreadUnretained(this), passed(std::move(task))));
  {
    MutexLocker lock(m_threadStateMutex);
    if (isolate() && m_threadState != ThreadState::ReadyToShutdown)
      m_inspectorTaskRunner->interruptAndRunAllTasksDontWait(isolate());
  }
  workerBackingThread().backingThread().postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&WorkerThread::performDebuggerTaskDontWaitOnWorkerThread,
                      crossThreadUnretained(this)));
}

void WorkerThread::startRunningDebuggerTasksOnPauseOnWorkerThread() {
  DCHECK(isCurrentThread());
  if (m_workerInspectorController)
    m_workerInspectorController->flushProtocolNotifications();
  m_pausedInDebugger = true;
  ThreadDebugger::idleStarted(isolate());
  std::unique_ptr<CrossThreadClosure> task;
  do {
    {
      SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);
      task =
          m_inspectorTaskRunner->takeNextTask(InspectorTaskRunner::WaitForTask);
    }
    if (task)
      (*task)();
    // Keep waiting until execution is resumed.
  } while (task && m_pausedInDebugger);
  ThreadDebugger::idleFinished(isolate());
}

void WorkerThread::stopRunningDebuggerTasksOnPauseOnWorkerThread() {
  DCHECK(isCurrentThread());
  m_pausedInDebugger = false;
}

WorkerOrWorkletGlobalScope* WorkerThread::globalScope() {
  DCHECK(isCurrentThread());
  return m_globalScope.get();
}

WorkerInspectorController* WorkerThread::workerInspectorController() {
  DCHECK(isCurrentThread());
  return m_workerInspectorController.get();
}

unsigned WorkerThread::workerThreadCount() {
  MutexLocker lock(threadSetMutex());
  return workerThreads().size();
}

HashSet<WorkerThread*>& WorkerThread::workerThreads() {
  DCHECK(isMainThread());
  DEFINE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
  return threads;
}

PlatformThreadId WorkerThread::platformThreadId() {
  if (!m_requestedToStart)
    return 0;
  return workerBackingThread().backingThread().platformThread().threadId();
}

bool WorkerThread::isForciblyTerminated() {
  MutexLocker lock(m_threadStateMutex);
  switch (m_exitCode) {
    case ExitCode::NotTerminated:
    case ExitCode::GracefullyTerminated:
      return false;
    case ExitCode::SyncForciblyTerminated:
    case ExitCode::AsyncForciblyTerminated:
      return true;
    case ExitCode::LastEnum:
      NOTREACHED() << static_cast<int>(m_exitCode);
      return false;
  }
  NOTREACHED() << static_cast<int>(m_exitCode);
  return false;
}

WorkerThread::WorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
                           WorkerReportingProxy& workerReportingProxy)
    : m_workerThreadId(getNextWorkerThreadId()),
      m_forcibleTerminationDelayInMs(kForcibleTerminationDelayInMs),
      m_inspectorTaskRunner(makeUnique<InspectorTaskRunner>()),
      m_workerLoaderProxy(workerLoaderProxy),
      m_workerReportingProxy(workerReportingProxy),
      m_shutdownEvent(wrapUnique(
          new WaitableEvent(WaitableEvent::ResetPolicy::Manual,
                            WaitableEvent::InitialState::NonSignaled))),
      m_workerThreadLifecycleContext(new WorkerThreadLifecycleContext) {
  DCHECK(isMainThread());
  MutexLocker lock(threadSetMutex());
  workerThreads().add(this);
}

void WorkerThread::terminateInternal(TerminationMode mode) {
  DCHECK(isMainThread());
  DCHECK(m_requestedToStart);

  {
    // Prevent the deadlock between GC and an attempt to terminate a thread.
    SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);

    // Protect against this method, initializeOnWorkerThread() or
    // termination via the global scope racing each other.
    MutexLocker lock(m_threadStateMutex);

    // If terminate has already been called.
    if (m_requestedToTerminate) {
      if (m_runningDebuggerTask) {
        // Any debugger task is guaranteed to finish, so we can wait
        // for the completion even if the synchronous forcible
        // termination is requested. Shutdown sequence will start
        // after the task.
        DCHECK(!m_forcibleTerminationTaskHandle.isActive());
        return;
      }

      // The synchronous forcible termination request should overtake the
      // scheduled termination task because the request will block the
      // main thread and the scheduled termination task never runs.
      if (mode == TerminationMode::Forcible &&
          m_exitCode == ExitCode::NotTerminated) {
        DCHECK(m_forcibleTerminationTaskHandle.isActive());
        forciblyTerminateExecution(lock, ExitCode::SyncForciblyTerminated);
      }
      return;
    }
    m_requestedToTerminate = true;

    if (shouldScheduleToTerminateExecution(lock)) {
      switch (mode) {
        case TerminationMode::Forcible:
          forciblyTerminateExecution(lock, ExitCode::SyncForciblyTerminated);
          break;
        case TerminationMode::Graceful:
          DCHECK(!m_forcibleTerminationTaskHandle.isActive());
          m_forcibleTerminationTaskHandle =
              Platform::current()
                  ->mainThread()
                  ->getWebTaskRunner()
                  ->postDelayedCancellableTask(
                      BLINK_FROM_HERE,
                      WTF::bind(&WorkerThread::mayForciblyTerminateExecution,
                                WTF::unretained(this)),
                      m_forcibleTerminationDelayInMs);
          break;
      }
    }
  }

  m_workerThreadLifecycleContext->notifyContextDestroyed();
  m_inspectorTaskRunner->kill();

  workerBackingThread().backingThread().postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&WorkerThread::prepareForShutdownOnWorkerThread,
                      crossThreadUnretained(this)));
  workerBackingThread().backingThread().postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&WorkerThread::performShutdownOnWorkerThread,
                      crossThreadUnretained(this)));
}

bool WorkerThread::shouldScheduleToTerminateExecution(const MutexLocker& lock) {
  DCHECK(isMainThread());
  DCHECK(isThreadStateMutexLocked(lock));

  switch (m_threadState) {
    case ThreadState::NotStarted:
      // Shutdown sequence will surely start during initialization sequence
      // on the worker thread. Don't have to schedule a termination task.
      return false;
    case ThreadState::Running:
      // Terminating during debugger task may lead to crash due to heavy use
      // of v8 api in debugger. Any debugger task is guaranteed to finish, so
      // we can wait for the completion.
      return !m_runningDebuggerTask;
    case ThreadState::ReadyToShutdown:
      // Shutdown sequence will surely start soon. Don't have to schedule a
      // termination task.
      return false;
  }
  NOTREACHED();
  return false;
}

void WorkerThread::mayForciblyTerminateExecution() {
  DCHECK(isMainThread());
  MutexLocker lock(m_threadStateMutex);
  if (m_threadState == ThreadState::ReadyToShutdown) {
    // Shutdown sequence is now running. Just return.
    return;
  }
  if (m_runningDebuggerTask) {
    // Any debugger task is guaranteed to finish, so we can wait for the
    // completion. Shutdown sequence will start after that.
    return;
  }

  forciblyTerminateExecution(lock, ExitCode::AsyncForciblyTerminated);
}

void WorkerThread::forciblyTerminateExecution(const MutexLocker& lock,
                                              ExitCode exitCode) {
  DCHECK(isMainThread());
  DCHECK(isThreadStateMutexLocked(lock));

  DCHECK(exitCode == ExitCode::SyncForciblyTerminated ||
         exitCode == ExitCode::AsyncForciblyTerminated);
  setExitCode(lock, exitCode);

  isolate()->TerminateExecution();
  m_forcibleTerminationTaskHandle.cancel();
}

bool WorkerThread::isInShutdown() {
  // Check if we've started termination or shutdown sequence. Avoid acquiring
  // a lock here to avoid introducing a risk of deadlock. Note that accessing
  // |m_requestedToTerminate| on the main thread or |m_threadState| on the
  // worker thread is safe as the flag is set only on the thread.
  if (isMainThread() && m_requestedToTerminate)
    return true;
  if (isCurrentThread() && m_threadState == ThreadState::ReadyToShutdown)
    return true;
  return false;
}

void WorkerThread::initializeOnWorkerThread(
    std::unique_ptr<WorkerThreadStartupData> startupData) {
  DCHECK(isCurrentThread());
  DCHECK_EQ(ThreadState::NotStarted, m_threadState);

  KURL scriptURL = startupData->m_scriptURL;
  String sourceCode = startupData->m_sourceCode;
  WorkerThreadStartMode startMode = startupData->m_startMode;
  std::unique_ptr<Vector<char>> cachedMetaData =
      std::move(startupData->m_cachedMetaData);
  V8CacheOptions v8CacheOptions = startupData->m_v8CacheOptions;

  {
    MutexLocker lock(m_threadStateMutex);

    if (isOwningBackingThread())
      workerBackingThread().initialize();
    workerBackingThread().backingThread().addTaskObserver(this);

    // Optimize for memory usage instead of latency for the worker isolate.
    isolate()->IsolateInBackgroundNotification();

    m_consoleMessageStorage = new ConsoleMessageStorage();
    m_globalScope = createWorkerGlobalScope(std::move(startupData));
    m_workerReportingProxy.didCreateWorkerGlobalScope(globalScope());
    m_workerInspectorController = WorkerInspectorController::create(this);

    // TODO(nhiroki): Handle a case where the script controller fails to
    // initialize the context.
    if (globalScope()->scriptController()->initializeContextIfNeeded()) {
      m_workerReportingProxy.didInitializeWorkerContext();
      v8::HandleScope handleScope(isolate());
      Platform::current()->workerContextCreated(
          globalScope()->scriptController()->context());
    }

    setThreadState(lock, ThreadState::Running);
  }

  if (startMode == PauseWorkerGlobalScopeOnStart)
    startRunningDebuggerTasksOnPauseOnWorkerThread();

  if (checkRequestedToTerminateOnWorkerThread()) {
    // Stop further worker tasks from running after this point. WorkerThread
    // was requested to terminate before initialization or during running
    // debugger tasks. performShutdownOnWorkerThread() will be called soon.
    prepareForShutdownOnWorkerThread();
    return;
  }

  if (globalScope()->isWorkerGlobalScope()) {
    WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(globalScope());
    CachedMetadataHandler* handler =
        workerGlobalScope->createWorkerScriptCachedMetadataHandler(
            scriptURL, cachedMetaData.get());
    m_workerReportingProxy.willEvaluateWorkerScript(
        sourceCode.length(), cachedMetaData.get() ? cachedMetaData->size() : 0);
    bool success = workerGlobalScope->scriptController()->evaluate(
        ScriptSourceCode(sourceCode, scriptURL), nullptr, handler,
        v8CacheOptions);
    m_workerReportingProxy.didEvaluateWorkerScript(success);
  }
}

void WorkerThread::prepareForShutdownOnWorkerThread() {
  DCHECK(isCurrentThread());
  {
    MutexLocker lock(m_threadStateMutex);
    if (m_threadState == ThreadState::ReadyToShutdown)
      return;
    setThreadState(lock, ThreadState::ReadyToShutdown);
    if (m_exitCode == ExitCode::NotTerminated)
      setExitCode(lock, ExitCode::GracefullyTerminated);
  }

  m_inspectorTaskRunner->kill();
  workerReportingProxy().willDestroyWorkerGlobalScope();
  InspectorInstrumentation::allAsyncTasksCanceled(globalScope());

  globalScope()->notifyContextDestroyed();
  if (m_workerInspectorController) {
    m_workerInspectorController->dispose();
    m_workerInspectorController.clear();
  }
  globalScope()->dispose();
  m_consoleMessageStorage.clear();
  workerBackingThread().backingThread().removeTaskObserver(this);
}

void WorkerThread::performShutdownOnWorkerThread() {
  DCHECK(isCurrentThread());
  DCHECK(checkRequestedToTerminateOnWorkerThread());
  DCHECK_EQ(ThreadState::ReadyToShutdown, m_threadState);

  // The below assignment will destroy the context, which will in turn notify
  // messaging proxy. We cannot let any objects survive past thread exit,
  // because no other thread will run GC or otherwise destroy them. If Oilpan
  // is enabled, we detach of the context/global scope, with the final heap
  // cleanup below sweeping it out.
  m_globalScope = nullptr;

  if (isOwningBackingThread())
    workerBackingThread().shutdown();
  // We must not touch workerBackingThread() from now on.

  // Notify the proxy that the WorkerOrWorkletGlobalScope has been disposed
  // of. This can free this thread object, hence it must not be touched
  // afterwards.
  workerReportingProxy().didTerminateWorkerThread();

  m_shutdownEvent->signal();
}

void WorkerThread::performTaskOnWorkerThread(
    std::unique_ptr<ExecutionContextTask> task,
    bool isInstrumented) {
  DCHECK(isCurrentThread());
  if (m_threadState != ThreadState::Running)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(globalScope(), task.get(),
                                                isInstrumented);
  {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scopedUsCounter,
        new CustomCountHistogram("WorkerThread.Task.Time", 0, 10000000, 50));
    ScopedUsHistogramTimer timer(scopedUsCounter);
    task->performTask(globalScope());
  }
}

void WorkerThread::performDebuggerTaskOnWorkerThread(
    std::unique_ptr<CrossThreadClosure> task) {
  DCHECK(isCurrentThread());
  InspectorTaskRunner::IgnoreInterruptsScope scope(m_inspectorTaskRunner.get());
  {
    MutexLocker lock(m_threadStateMutex);
    DCHECK_EQ(ThreadState::Running, m_threadState);
    m_runningDebuggerTask = true;
  }
  ThreadDebugger::idleFinished(isolate());
  {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scopedUsCounter,
        new CustomCountHistogram("WorkerThread.DebuggerTask.Time", 0, 10000000,
                                 50));
    ScopedUsHistogramTimer timer(scopedUsCounter);
    (*task)();
  }
  ThreadDebugger::idleStarted(isolate());
  {
    MutexLocker lock(m_threadStateMutex);
    m_runningDebuggerTask = false;
    if (!m_requestedToTerminate)
      return;
    // termiante() was called while a debugger task is running. Shutdown
    // sequence will start soon.
  }
  // Stop further debugger tasks from running after this point.
  m_inspectorTaskRunner->kill();
}

void WorkerThread::performDebuggerTaskDontWaitOnWorkerThread() {
  DCHECK(isCurrentThread());
  std::unique_ptr<CrossThreadClosure> task =
      m_inspectorTaskRunner->takeNextTask(InspectorTaskRunner::DontWaitForTask);
  if (task)
    (*task)();
}

void WorkerThread::setThreadState(const MutexLocker& lock,
                                  ThreadState nextThreadState) {
  DCHECK(isThreadStateMutexLocked(lock));
  switch (nextThreadState) {
    case ThreadState::NotStarted:
      NOTREACHED();
      return;
    case ThreadState::Running:
      DCHECK_EQ(ThreadState::NotStarted, m_threadState);
      m_threadState = nextThreadState;
      return;
    case ThreadState::ReadyToShutdown:
      DCHECK_EQ(ThreadState::Running, m_threadState);
      m_threadState = nextThreadState;
      return;
  }
}

void WorkerThread::setExitCode(const MutexLocker& lock, ExitCode exitCode) {
  DCHECK(isThreadStateMutexLocked(lock));
  DCHECK_EQ(ExitCode::NotTerminated, m_exitCode);
  m_exitCode = exitCode;
}

bool WorkerThread::isThreadStateMutexLocked(const MutexLocker& /* unused */) {
#if ENABLE(ASSERT)
  // Mutex::locked() is available only if ENABLE(ASSERT) is true.
  return m_threadStateMutex.locked();
#else
  // Otherwise, believe the given MutexLocker holds |m_threadStateMutex|.
  return true;
#endif
}

bool WorkerThread::checkRequestedToTerminateOnWorkerThread() {
  MutexLocker lock(m_threadStateMutex);
  return m_requestedToTerminate;
}

ExitCode WorkerThread::getExitCodeForTesting() {
  MutexLocker lock(m_threadStateMutex);
  return m_exitCode;
}

}  // namespace blink
