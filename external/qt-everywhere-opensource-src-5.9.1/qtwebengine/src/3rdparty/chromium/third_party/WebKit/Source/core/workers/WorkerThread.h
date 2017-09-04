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
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "platform/LifecycleNotifier.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"
#include "wtf/PassRefPtr.h"
#include <memory>
#include <v8.h>

namespace blink {

class ConsoleMessageStorage;
class InspectorTaskRunner;
class WorkerBackingThread;
class WorkerInspectorController;
class WorkerOrWorkletGlobalScope;
class WorkerReportingProxy;
class WorkerThreadStartupData;

enum WorkerThreadStartMode {
  DontPauseWorkerGlobalScopeOnStart,
  PauseWorkerGlobalScopeOnStart
};

// Used for notifying observers on the main thread of worker thread termination.
// The lifetime of this class is equal to that of WorkerThread. Created and
// destructed on the main thread.
class CORE_EXPORT WorkerThreadLifecycleContext final
    : public GarbageCollectedFinalized<WorkerThreadLifecycleContext>,
      public LifecycleNotifier<WorkerThreadLifecycleContext,
                               WorkerThreadLifecycleObserver> {
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
class CORE_EXPORT WorkerThread : public WebThread::TaskObserver {
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

  // WebThread::TaskObserver.
  void willProcessTask() override;
  void didProcessTask() override;

  virtual WorkerBackingThread& workerBackingThread() = 0;
  virtual void clearWorkerBackingThread() = 0;
  ConsoleMessageStorage* consoleMessageStorage() const {
    return m_consoleMessageStorage.get();
  }
  v8::Isolate* isolate();

  bool isCurrentThread();

  WorkerLoaderProxy* workerLoaderProxy() const {
    RELEASE_ASSERT(m_workerLoaderProxy);
    return m_workerLoaderProxy.get();
  }

  WorkerReportingProxy& workerReportingProxy() const {
    return m_workerReportingProxy;
  }

  void postTask(const WebTraceLocation&,
                std::unique_ptr<ExecutionContextTask>,
                bool isInstrumented = false);
  void appendDebuggerTask(std::unique_ptr<CrossThreadClosure>);

  // Runs only debugger tasks while paused in debugger.
  void startRunningDebuggerTasksOnPauseOnWorkerThread();
  void stopRunningDebuggerTasksOnPauseOnWorkerThread();

  // Can be called only on the worker thread, WorkerOrWorkletGlobalScope
  // and WorkerInspectorController are not thread safe.
  WorkerOrWorkletGlobalScope* globalScope();
  WorkerInspectorController* workerInspectorController();

  // Called for creating WorkerThreadLifecycleObserver on both the main thread
  // and the worker thread.
  WorkerThreadLifecycleContext* getWorkerThreadLifecycleContext() const {
    return m_workerThreadLifecycleContext;
  }

  // Number of active worker threads.
  static unsigned workerThreadCount();

  // Returns a set of all worker threads. This must be called only on the main
  // thread and the returned set must not be stored for future use.
  static HashSet<WorkerThread*>& workerThreads();

  int getWorkerThreadId() const { return m_workerThreadId; }

  PlatformThreadId platformThreadId();

  bool isForciblyTerminated();

  void waitForShutdownForTesting() { m_shutdownEvent->wait(); }

 protected:
  WorkerThread(PassRefPtr<WorkerLoaderProxy>, WorkerReportingProxy&);

  // Factory method for creating a new worker context for the thread.
  // Called on the worker thread.
  virtual WorkerOrWorkletGlobalScope* createWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData>) = 0;

  // Returns true when this WorkerThread owns the associated
  // WorkerBackingThread exclusively. If this function returns true, the
  // WorkerThread initializes / shutdowns the backing thread. Otherwise
  // workerBackingThread() should be initialized / shutdown properly
  // out of this class.
  virtual bool isOwningBackingThread() const { return true; }

 private:
  friend class WorkerThreadTest;
  FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest,
                           ShouldScheduleToTerminateExecution);
  FRIEND_TEST_ALL_PREFIXES(
      WorkerThreadTest,
      Terminate_WhileDebuggerTaskIsRunningOnInitialization);
  FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest,
                           Terminate_WhileDebuggerTaskIsRunning);

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

  // Represents the state of this worker thread. A caller may need to acquire
  // a lock |m_threadStateMutex| before accessing this:
  //   - Only the worker thread can set this with the lock.
  //   - The worker thread can read this without the lock.
  //   - The main thread can read this with the lock.
  enum class ThreadState {
    NotStarted,
    Running,
    ReadyToShutdown,
  };

  void terminateInternal(TerminationMode);

  // Returns true if we should synchronously terminate or schedule to
  // terminate the worker execution so that a shutdown task can be handled by
  // the thread event loop. This must be called with |m_threadStateMutex|
  // acquired.
  bool shouldScheduleToTerminateExecution(const MutexLocker&);

  // Called as a delayed task to terminate the worker execution from the main
  // thread. This task is expected to run when the shutdown sequence does not
  // start in a certain time period because of an inifite loop in the JS
  // execution context etc. When the shutdown sequence is started before this
  // task runs, the task is simply cancelled.
  void mayForciblyTerminateExecution();

  // Forcibly terminates the worker execution. This must be called with
  // |m_threadStateMutex| acquired.
  void forciblyTerminateExecution(const MutexLocker&, ExitCode);

  // Returns true if termination or shutdown sequence has started. This is
  // thread safe.
  // Note that this returns false when the sequence has already started but it
  // hasn't been notified to the calling thread.
  bool isInShutdown();

  void initializeOnWorkerThread(std::unique_ptr<WorkerThreadStartupData>);
  void prepareForShutdownOnWorkerThread();
  void performShutdownOnWorkerThread();
  void performTaskOnWorkerThread(std::unique_ptr<ExecutionContextTask>,
                                 bool isInstrumented);
  void performDebuggerTaskOnWorkerThread(std::unique_ptr<CrossThreadClosure>);
  void performDebuggerTaskDontWaitOnWorkerThread();

  // These must be called with |m_threadStateMutex| acquired.
  void setThreadState(const MutexLocker&, ThreadState);
  void setExitCode(const MutexLocker&, ExitCode);
  bool isThreadStateMutexLocked(const MutexLocker&);

  // This internally acquires |m_threadStateMutex|. If you already have the
  // lock or you're on the main thread, you should consider directly accessing
  // |m_requestedToTerminate|.
  bool checkRequestedToTerminateOnWorkerThread();

  ExitCode getExitCodeForTesting();

  // A unique identifier among all WorkerThreads.
  const int m_workerThreadId;

  // Accessed only on the main thread.
  bool m_requestedToStart = false;

  // Set on the main thread and checked on both the main and worker threads.
  bool m_requestedToTerminate = false;

  // Accessed only on the worker thread.
  bool m_pausedInDebugger = false;

  // Set on the worker thread and checked on both the main and worker threads.
  bool m_runningDebuggerTask = false;

  ThreadState m_threadState = ThreadState::NotStarted;
  ExitCode m_exitCode = ExitCode::NotTerminated;

  long long m_forcibleTerminationDelayInMs;

  std::unique_ptr<InspectorTaskRunner> m_inspectorTaskRunner;

  RefPtr<WorkerLoaderProxy> m_workerLoaderProxy;
  WorkerReportingProxy& m_workerReportingProxy;

  // This lock protects |m_globalScope|, |m_requestedToTerminate|,
  // |m_threadState|, |m_runningDebuggerTask| and |m_exitCode|.
  Mutex m_threadStateMutex;

  CrossThreadPersistent<ConsoleMessageStorage> m_consoleMessageStorage;
  CrossThreadPersistent<WorkerOrWorkletGlobalScope> m_globalScope;
  CrossThreadPersistent<WorkerInspectorController> m_workerInspectorController;

  // Signaled when the thread completes termination on the worker thread.
  std::unique_ptr<WaitableEvent> m_shutdownEvent;

  // Used to cancel a scheduled forcible termination task. See
  // mayForciblyTerminateExecution() for details.
  TaskHandle m_forcibleTerminationTaskHandle;

  Persistent<WorkerThreadLifecycleContext> m_workerThreadLifecycleContext;
};

}  // namespace blink

#endif  // WorkerThread_h
