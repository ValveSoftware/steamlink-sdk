// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThread.h"

#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/WaitableEvent.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

using testing::_;
using testing::AtMost;

namespace blink {

using ExitCode = WorkerThread::ExitCode;

namespace {

// Used as a debugger task. Waits for a signal from the main thread.
void waitForSignalTask(WorkerThread* workerThread,
                       WaitableEvent* waitableEvent) {
  EXPECT_TRUE(workerThread->isCurrentThread());

  // Notify the main thread that the debugger task is waiting for the signal.
  Platform::current()->mainThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, crossThreadBind(&testing::exitRunLoop));
  waitableEvent->wait();
}

}  // namespace

class WorkerThreadTest
    : public ::testing::TestWithParam<BlinkGC::ThreadHeapMode> {
 public:
  WorkerThreadTest() : m_threadHeapMode(GetParam()) {}

  void SetUp() override {
    m_loaderProxyProvider = makeUnique<MockWorkerLoaderProxyProvider>();
    m_reportingProxy = makeUnique<MockWorkerReportingProxy>();
    m_securityOrigin =
        SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
    m_workerThread = wrapUnique(new WorkerThreadForTest(
        m_loaderProxyProvider.get(), *m_reportingProxy, m_threadHeapMode));
    m_lifecycleObserver = new MockWorkerThreadLifecycleObserver(
        m_workerThread->getWorkerThreadLifecycleContext());
  }

  void TearDown() override {
    m_workerThread->workerLoaderProxy()->detachProvider(
        m_loaderProxyProvider.get());
  }

  void start() {
    m_workerThread->startWithSourceCode(m_securityOrigin.get(),
                                        "//fake source code");
  }

  void startWithSourceCodeNotToFinish() {
    // Use a JavaScript source code that makes an infinite loop so that we
    // can catch some kind of issues as a timeout.
    m_workerThread->startWithSourceCode(m_securityOrigin.get(),
                                        "while(true) {}");
  }

  void setForcibleTerminationDelayInMs(long long forcibleTerminationDelayInMs) {
    m_workerThread->m_forcibleTerminationDelayInMs =
        forcibleTerminationDelayInMs;
  }

  bool isForcibleTerminationTaskScheduled() {
    return m_workerThread->m_forcibleTerminationTaskHandle.isActive();
  }

 protected:
  void expectReportingCalls() {
    EXPECT_CALL(*m_reportingProxy, didCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*m_reportingProxy, didInitializeWorkerContext()).Times(1);
    EXPECT_CALL(*m_reportingProxy, willEvaluateWorkerScriptMock(_, _)).Times(1);
    EXPECT_CALL(*m_reportingProxy, didEvaluateWorkerScript(true)).Times(1);
    EXPECT_CALL(*m_reportingProxy, willDestroyWorkerGlobalScope()).Times(1);
    EXPECT_CALL(*m_reportingProxy, didTerminateWorkerThread()).Times(1);
    EXPECT_CALL(*m_lifecycleObserver, contextDestroyed()).Times(1);
  }

  void expectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization() {
    EXPECT_CALL(*m_reportingProxy, didCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*m_reportingProxy, didInitializeWorkerContext())
        .Times(AtMost(1));
    EXPECT_CALL(*m_reportingProxy, willEvaluateWorkerScriptMock(_, _))
        .Times(AtMost(1));
    EXPECT_CALL(*m_reportingProxy, didEvaluateWorkerScript(_)).Times(AtMost(1));
    EXPECT_CALL(*m_reportingProxy, willDestroyWorkerGlobalScope())
        .Times(AtMost(1));
    EXPECT_CALL(*m_reportingProxy, didTerminateWorkerThread()).Times(1);
    EXPECT_CALL(*m_lifecycleObserver, contextDestroyed()).Times(1);
  }

  void expectReportingCallsForWorkerForciblyTerminated() {
    EXPECT_CALL(*m_reportingProxy, didCreateWorkerGlobalScope(_)).Times(1);
    EXPECT_CALL(*m_reportingProxy, didInitializeWorkerContext()).Times(1);
    EXPECT_CALL(*m_reportingProxy, willEvaluateWorkerScriptMock(_, _)).Times(1);
    EXPECT_CALL(*m_reportingProxy, didEvaluateWorkerScript(false)).Times(1);
    EXPECT_CALL(*m_reportingProxy, willDestroyWorkerGlobalScope()).Times(1);
    EXPECT_CALL(*m_reportingProxy, didTerminateWorkerThread()).Times(1);
    EXPECT_CALL(*m_lifecycleObserver, contextDestroyed()).Times(1);
  }

  ExitCode getExitCode() { return m_workerThread->getExitCodeForTesting(); }

  RefPtr<SecurityOrigin> m_securityOrigin;
  std::unique_ptr<MockWorkerLoaderProxyProvider> m_loaderProxyProvider;
  std::unique_ptr<MockWorkerReportingProxy> m_reportingProxy;
  std::unique_ptr<WorkerThreadForTest> m_workerThread;
  Persistent<MockWorkerThreadLifecycleObserver> m_lifecycleObserver;
  const BlinkGC::ThreadHeapMode m_threadHeapMode;
};

INSTANTIATE_TEST_CASE_P(MainThreadHeap,
                        WorkerThreadTest,
                        ::testing::Values(BlinkGC::MainThreadHeapMode));

INSTANTIATE_TEST_CASE_P(PerThreadHeap,
                        WorkerThreadTest,
                        ::testing::Values(BlinkGC::PerThreadHeapMode));

TEST_P(WorkerThreadTest, ShouldScheduleToTerminateExecution) {
  using ThreadState = WorkerThread::ThreadState;
  MutexLocker dummyLock(m_workerThread->m_threadStateMutex);

  EXPECT_EQ(ThreadState::NotStarted, m_workerThread->m_threadState);
  EXPECT_FALSE(m_workerThread->shouldScheduleToTerminateExecution(dummyLock));

  m_workerThread->setThreadState(dummyLock, ThreadState::Running);
  EXPECT_FALSE(m_workerThread->m_runningDebuggerTask);
  EXPECT_TRUE(m_workerThread->shouldScheduleToTerminateExecution(dummyLock));

  m_workerThread->m_runningDebuggerTask = true;
  EXPECT_FALSE(m_workerThread->shouldScheduleToTerminateExecution(dummyLock));

  m_workerThread->setThreadState(dummyLock, ThreadState::ReadyToShutdown);
  EXPECT_FALSE(m_workerThread->shouldScheduleToTerminateExecution(dummyLock));

  // This is necessary to satisfy DCHECK in the dtor of WorkerThread.
  m_workerThread->setExitCode(dummyLock, ExitCode::GracefullyTerminated);
}

TEST_P(WorkerThreadTest, AsyncTerminate_OnIdle) {
  expectReportingCalls();
  start();

  // Wait until the initialization completes and the worker thread becomes
  // idle.
  m_workerThread->waitForInit();

  // The worker thread is not being blocked, so the worker thread should be
  // gracefully shut down.
  m_workerThread->terminate();
  EXPECT_TRUE(isForcibleTerminationTaskScheduled());
  m_workerThread->waitForShutdownForTesting();
  EXPECT_EQ(ExitCode::GracefullyTerminated, getExitCode());
}

TEST_P(WorkerThreadTest, SyncTerminate_OnIdle) {
  expectReportingCalls();
  start();

  // Wait until the initialization completes and the worker thread becomes
  // idle.
  m_workerThread->waitForInit();

  m_workerThread->terminateAndWait();
  EXPECT_EQ(ExitCode::SyncForciblyTerminated, getExitCode());
}

TEST_P(WorkerThreadTest, AsyncTerminate_ImmediatelyAfterStart) {
  expectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
  start();

  // The worker thread is not being blocked, so the worker thread should be
  // gracefully shut down.
  m_workerThread->terminate();
  m_workerThread->waitForShutdownForTesting();
  EXPECT_EQ(ExitCode::GracefullyTerminated, getExitCode());
}

TEST_P(WorkerThreadTest, SyncTerminate_ImmediatelyAfterStart) {
  expectReportingCallsForWorkerPossiblyTerminatedBeforeInitialization();
  start();

  // There are two possible cases depending on timing:
  // (1) If the thread hasn't been initialized on the worker thread yet,
  // terminateAndWait() should wait for initialization and shut down the
  // thread immediately after that.
  // (2) If the thread has already been initialized on the worker thread,
  // terminateAndWait() should synchronously forcibly terminates the worker
  // execution.
  // TODO(nhiroki): Make this test deterministically pass through the case 1),
  // that is, terminateAndWait() is called before initializeOnWorkerThread().
  // Then, rename this test to SyncTerminate_BeforeInitialization.
  m_workerThread->terminateAndWait();
  ExitCode exitCode = getExitCode();
  EXPECT_TRUE(ExitCode::GracefullyTerminated == exitCode ||
              ExitCode::SyncForciblyTerminated == exitCode);
}

TEST_P(WorkerThreadTest, AsyncTerminate_WhileTaskIsRunning) {
  const long long kForcibleTerminationDelayInMs = 10;
  setForcibleTerminationDelayInMs(kForcibleTerminationDelayInMs);

  expectReportingCallsForWorkerForciblyTerminated();
  startWithSourceCodeNotToFinish();
  m_reportingProxy->waitUntilScriptEvaluation();

  // terminate() schedules a force termination task.
  m_workerThread->terminate();
  EXPECT_TRUE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Multiple terminate() calls should not take effect.
  m_workerThread->terminate();
  m_workerThread->terminate();
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Wait until the force termination task runs.
  testing::runDelayedTasks(kForcibleTerminationDelayInMs);
  m_workerThread->waitForShutdownForTesting();
  EXPECT_EQ(ExitCode::AsyncForciblyTerminated, getExitCode());
}

TEST_P(WorkerThreadTest, SyncTerminate_WhileTaskIsRunning) {
  expectReportingCallsForWorkerForciblyTerminated();
  startWithSourceCodeNotToFinish();
  m_reportingProxy->waitUntilScriptEvaluation();

  // terminateAndWait() synchronously terminates the worker execution.
  m_workerThread->terminateAndWait();
  EXPECT_EQ(ExitCode::SyncForciblyTerminated, getExitCode());
}

TEST_P(WorkerThreadTest,
       AsyncTerminateAndThenSyncTerminate_WhileTaskIsRunning) {
  const long long kForcibleTerminationDelayInMs = 10;
  setForcibleTerminationDelayInMs(kForcibleTerminationDelayInMs);

  expectReportingCallsForWorkerForciblyTerminated();
  startWithSourceCodeNotToFinish();
  m_reportingProxy->waitUntilScriptEvaluation();

  // terminate() schedules a force termination task.
  m_workerThread->terminate();
  EXPECT_TRUE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // terminateAndWait() should overtake the scheduled force termination task.
  m_workerThread->terminateAndWait();
  EXPECT_FALSE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::SyncForciblyTerminated, getExitCode());
}

TEST_P(WorkerThreadTest, Terminate_WhileDebuggerTaskIsRunningOnInitialization) {
  EXPECT_CALL(*m_reportingProxy, didCreateWorkerGlobalScope(_)).Times(1);
  EXPECT_CALL(*m_reportingProxy, didInitializeWorkerContext()).Times(1);
  EXPECT_CALL(*m_reportingProxy, willDestroyWorkerGlobalScope()).Times(1);
  EXPECT_CALL(*m_reportingProxy, didTerminateWorkerThread()).Times(1);
  EXPECT_CALL(*m_lifecycleObserver, contextDestroyed()).Times(1);

  std::unique_ptr<Vector<CSPHeaderAndType>> headers =
      makeUnique<Vector<CSPHeaderAndType>>();
  CSPHeaderAndType headerAndType("contentSecurityPolicy",
                                 ContentSecurityPolicyHeaderTypeReport);
  headers->append(headerAndType);

  // Specify PauseWorkerGlobalScopeOnStart so that the worker thread can pause
  // on initialziation to run debugger tasks.
  std::unique_ptr<WorkerThreadStartupData> startupData =
      WorkerThreadStartupData::create(
          KURL(ParsedURLString, "http://fake.url/"), "fake user agent",
          "//fake source code", nullptr, /* cachedMetaData */
          PauseWorkerGlobalScopeOnStart, headers.get(), "",
          m_securityOrigin.get(), nullptr, /* workerClients */
          WebAddressSpaceLocal, nullptr /* originTrialToken */,
          nullptr /* WorkerSettings */, V8CacheOptionsDefault);
  m_workerThread->start(std::move(startupData));

  // Used to wait for worker thread termination in a debugger task on the
  // worker thread.
  WaitableEvent waitableEvent;
  m_workerThread->appendDebuggerTask(crossThreadBind(
      &waitForSignalTask, crossThreadUnretained(m_workerThread.get()),
      crossThreadUnretained(&waitableEvent)));

  // Wait for the debugger task.
  testing::enterRunLoop();
  EXPECT_TRUE(m_workerThread->m_runningDebuggerTask);

  // terminate() should not schedule a force termination task because there is
  // a running debugger task.
  m_workerThread->terminate();
  EXPECT_FALSE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Multiple terminate() calls should not take effect.
  m_workerThread->terminate();
  m_workerThread->terminate();
  EXPECT_FALSE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Focible termination request should also respect the running debugger
  // task.
  m_workerThread->terminateInternal(WorkerThread::TerminationMode::Forcible);
  EXPECT_FALSE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Resume the debugger task. Shutdown starts after that.
  waitableEvent.signal();
  m_workerThread->waitForShutdownForTesting();
  EXPECT_EQ(ExitCode::GracefullyTerminated, getExitCode());
}

TEST_P(WorkerThreadTest, Terminate_WhileDebuggerTaskIsRunning) {
  expectReportingCalls();
  start();
  m_workerThread->waitForInit();

  // Used to wait for worker thread termination in a debugger task on the
  // worker thread.
  WaitableEvent waitableEvent;
  m_workerThread->appendDebuggerTask(crossThreadBind(
      &waitForSignalTask, crossThreadUnretained(m_workerThread.get()),
      crossThreadUnretained(&waitableEvent)));

  // Wait for the debugger task.
  testing::enterRunLoop();
  EXPECT_TRUE(m_workerThread->m_runningDebuggerTask);

  // terminate() should not schedule a force termination task because there is
  // a running debugger task.
  m_workerThread->terminate();
  EXPECT_FALSE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Multiple terminate() calls should not take effect.
  m_workerThread->terminate();
  m_workerThread->terminate();
  EXPECT_FALSE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Focible termination request should also respect the running debugger
  // task.
  m_workerThread->terminateInternal(WorkerThread::TerminationMode::Forcible);
  EXPECT_FALSE(isForcibleTerminationTaskScheduled());
  EXPECT_EQ(ExitCode::NotTerminated, getExitCode());

  // Resume the debugger task. Shutdown starts after that.
  waitableEvent.signal();
  m_workerThread->waitForShutdownForTesting();
  EXPECT_EQ(ExitCode::GracefullyTerminated, getExitCode());
}

// TODO(nhiroki): Add tests for terminateAndWaitForAllWorkers.

}  // namespace blink
