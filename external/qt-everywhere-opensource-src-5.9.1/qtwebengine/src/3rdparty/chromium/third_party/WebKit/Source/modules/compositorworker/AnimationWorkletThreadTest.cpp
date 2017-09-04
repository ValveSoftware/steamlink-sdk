// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletThread.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {
namespace {

// A null WorkerReportingProxy, supplied when creating AnimationWorkletThreads.
class TestAnimationWorkletReportingProxy : public WorkerReportingProxy {
 public:
  static std::unique_ptr<TestAnimationWorkletReportingProxy> create() {
    return wrapUnique(new TestAnimationWorkletReportingProxy());
  }

  // (Empty) WorkerReportingProxy implementation:
  void reportException(const String& errorMessage,
                       std::unique_ptr<SourceLocation>,
                       int exceptionId) override {}
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override {}
  void postMessageToPageInspector(const String&) override {}

  void didEvaluateWorkerScript(bool success) override {}
  void didCloseWorkerGlobalScope() override {}
  void willDestroyWorkerGlobalScope() override {}
  void didTerminateWorkerThread() override {}

 private:
  TestAnimationWorkletReportingProxy() {}
};

class AnimationWorkletTestPlatform : public TestingPlatformSupport {
 public:
  AnimationWorkletTestPlatform()
      : m_thread(wrapUnique(m_oldPlatform->createThread("Compositor"))) {}

  WebThread* compositorThread() const override { return m_thread.get(); }

  WebCompositorSupport* compositorSupport() override {
    return &m_compositorSupport;
  }

 private:
  std::unique_ptr<WebThread> m_thread;
  TestingCompositorSupport m_compositorSupport;
};

}  // namespace

class AnimationWorkletThreadTest : public ::testing::Test {
 public:
  void SetUp() override {
    AnimationWorkletThread::createSharedBackingThreadForTest();
    m_reportingProxy = TestAnimationWorkletReportingProxy::create();
    m_securityOrigin =
        SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
  }

  void TearDown() override {
    AnimationWorkletThread::clearSharedBackingThread();
  }

  std::unique_ptr<AnimationWorkletThread> createAnimationWorkletThread() {
    std::unique_ptr<AnimationWorkletThread> thread =
        AnimationWorkletThread::create(nullptr, *m_reportingProxy);
    thread->start(WorkerThreadStartupData::create(
        KURL(ParsedURLString, "http://fake.url/"), "fake user agent", "",
        nullptr, DontPauseWorkerGlobalScopeOnStart, nullptr, "",
        m_securityOrigin.get(), nullptr, WebAddressSpaceLocal, nullptr, nullptr,
        V8CacheOptionsDefault));
    return thread;
  }

  // Attempts to run some simple script for |thread|.
  void checkWorkletCanExecuteScript(WorkerThread* thread) {
    std::unique_ptr<WaitableEvent> waitEvent = makeUnique<WaitableEvent>();
    thread->workerBackingThread().backingThread().postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&AnimationWorkletThreadTest::executeScriptInWorklet,
                        crossThreadUnretained(this),
                        crossThreadUnretained(thread),
                        crossThreadUnretained(waitEvent.get())));
    waitEvent->wait();
  }

 private:
  void executeScriptInWorklet(WorkerThread* thread, WaitableEvent* waitEvent) {
    WorkerOrWorkletScriptController* scriptController =
        thread->globalScope()->scriptController();
    scriptController->evaluate(ScriptSourceCode("var counter = 0; ++counter;"));
    waitEvent->signal();
  }

  RefPtr<SecurityOrigin> m_securityOrigin;
  std::unique_ptr<WorkerReportingProxy> m_reportingProxy;
  AnimationWorkletTestPlatform m_testPlatform;
};

TEST_F(AnimationWorkletThreadTest, Basic) {
  std::unique_ptr<AnimationWorkletThread> worklet =
      createAnimationWorkletThread();
  checkWorkletCanExecuteScript(worklet.get());
  worklet->terminateAndWait();
}

// Tests that the same WebThread is used for new worklets if the WebThread is
// still alive.
TEST_F(AnimationWorkletThreadTest, CreateSecondAndTerminateFirst) {
  // Create the first worklet and wait until it is initialized.
  std::unique_ptr<AnimationWorkletThread> firstWorklet =
      createAnimationWorkletThread();
  WebThreadSupportingGC* firstThread =
      &firstWorklet->workerBackingThread().backingThread();
  checkWorkletCanExecuteScript(firstWorklet.get());
  v8::Isolate* firstIsolate = firstWorklet->isolate();
  ASSERT_TRUE(firstIsolate);

  // Create the second worklet and immediately destroy the first worklet.
  std::unique_ptr<AnimationWorkletThread> secondWorklet =
      createAnimationWorkletThread();
  // We don't use terminateAndWait here to avoid forcible termination.
  firstWorklet->terminate();
  firstWorklet->waitForShutdownForTesting();

  // Wait until the second worklet is initialized. Verify that the second
  // worklet is using the same thread and Isolate as the first worklet.
  WebThreadSupportingGC* secondThread =
      &secondWorklet->workerBackingThread().backingThread();
  ASSERT_EQ(firstThread, secondThread);

  v8::Isolate* secondIsolate = secondWorklet->isolate();
  ASSERT_TRUE(secondIsolate);
  EXPECT_EQ(firstIsolate, secondIsolate);

  // Verify that the worklet can still successfully execute script.
  checkWorkletCanExecuteScript(secondWorklet.get());

  secondWorklet->terminateAndWait();
}

// Tests that a new WebThread is created if all existing worklets are
// terminated before a new worklet is created.
TEST_F(AnimationWorkletThreadTest, TerminateFirstAndCreateSecond) {
  // Create the first worklet, wait until it is initialized, and terminate it.
  std::unique_ptr<AnimationWorkletThread> worklet =
      createAnimationWorkletThread();
  WebThreadSupportingGC* firstThread =
      &worklet->workerBackingThread().backingThread();
  checkWorkletCanExecuteScript(worklet.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  worklet->terminate();
  worklet->waitForShutdownForTesting();

  // Create the second worklet. The backing thread is same.
  worklet = createAnimationWorkletThread();
  WebThreadSupportingGC* secondThread =
      &worklet->workerBackingThread().backingThread();
  EXPECT_EQ(firstThread, secondThread);
  checkWorkletCanExecuteScript(worklet.get());

  worklet->terminateAndWait();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worklet is
// created while another is terminating.
TEST_F(AnimationWorkletThreadTest, CreatingSecondDuringTerminationOfFirst) {
  std::unique_ptr<AnimationWorkletThread> firstWorklet =
      createAnimationWorkletThread();
  checkWorkletCanExecuteScript(firstWorklet.get());
  v8::Isolate* firstIsolate = firstWorklet->isolate();
  ASSERT_TRUE(firstIsolate);

  // Request termination of the first worklet and create the second worklet
  // as soon as possible.
  firstWorklet->terminate();
  // We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worklet thread so quickly. This could be a source of flakiness.

  std::unique_ptr<AnimationWorkletThread> secondWorklet =
      createAnimationWorkletThread();

  v8::Isolate* secondIsolate = secondWorklet->isolate();
  ASSERT_TRUE(secondIsolate);
  EXPECT_EQ(firstIsolate, secondIsolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worklet.
  checkWorkletCanExecuteScript(secondWorklet.get());
  secondWorklet->terminateAndWait();
}

}  // namespace blink
