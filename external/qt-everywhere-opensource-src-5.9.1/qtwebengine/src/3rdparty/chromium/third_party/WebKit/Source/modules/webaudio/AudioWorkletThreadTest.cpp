// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletThread.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
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

// A null WorkerReportingProxy, supplied when creating AudioWorkletThreads.
class TestAudioWorkletReportingProxy : public WorkerReportingProxy {
 public:
  static std::unique_ptr<TestAudioWorkletReportingProxy> create() {
    return wrapUnique(new TestAudioWorkletReportingProxy());
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
  TestAudioWorkletReportingProxy() {}
};

}  // namespace

class AudioWorkletThreadTest : public ::testing::Test {
 public:
  void SetUp() override {
    AudioWorkletThread::createSharedBackingThreadForTest();
    m_reportingProxy = TestAudioWorkletReportingProxy::create();
    m_securityOrigin =
        SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
  }

  void TearDown() override { AudioWorkletThread::clearSharedBackingThread(); }

  std::unique_ptr<AudioWorkletThread> createAudioWorkletThread() {
    std::unique_ptr<AudioWorkletThread> thread =
        AudioWorkletThread::create(nullptr, *m_reportingProxy);
    thread->start(WorkerThreadStartupData::create(
        KURL(ParsedURLString, "http://fake.url/"), "fake user agent", "",
        nullptr, DontPauseWorkerGlobalScopeOnStart, nullptr, "",
        m_securityOrigin.get(), nullptr, WebAddressSpaceLocal, nullptr, nullptr,
        V8CacheOptionsDefault));
    return thread;
  }

  // Attempts to run some simple script for |thread|.
  void checkWorkletCanExecuteScript(WorkerThread* thread) {
    WaitableEvent waitEvent;
    thread->workerBackingThread().backingThread().postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&AudioWorkletThreadTest::executeScriptInWorklet,
                        crossThreadUnretained(this),
                        crossThreadUnretained(thread),
                        crossThreadUnretained(&waitEvent)));
    waitEvent.wait();
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
};

TEST_F(AudioWorkletThreadTest, Basic) {
  std::unique_ptr<AudioWorkletThread> worklet = createAudioWorkletThread();
  checkWorkletCanExecuteScript(worklet.get());
  worklet->terminateAndWait();
}

// Tests that the same WebThread is used for new worklets if the WebThread is
// still alive.
TEST_F(AudioWorkletThreadTest, CreateSecondAndTerminateFirst) {
  // Create the first worklet and wait until it is initialized.
  std::unique_ptr<AudioWorkletThread> firstWorklet = createAudioWorkletThread();
  WebThreadSupportingGC* firstThread =
      &firstWorklet->workerBackingThread().backingThread();
  checkWorkletCanExecuteScript(firstWorklet.get());
  v8::Isolate* firstIsolate = firstWorklet->isolate();
  ASSERT_TRUE(firstIsolate);

  // Create the second worklet and immediately destroy the first worklet.
  std::unique_ptr<AudioWorkletThread> secondWorklet =
      createAudioWorkletThread();
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
TEST_F(AudioWorkletThreadTest, TerminateFirstAndCreateSecond) {
  // Create the first worklet, wait until it is initialized, and terminate it.
  std::unique_ptr<AudioWorkletThread> worklet = createAudioWorkletThread();
  WebThreadSupportingGC* firstThread =
      &worklet->workerBackingThread().backingThread();
  checkWorkletCanExecuteScript(worklet.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  worklet->terminate();
  worklet->waitForShutdownForTesting();

  // Create the second worklet. The backing thread is same.
  worklet = createAudioWorkletThread();
  WebThreadSupportingGC* secondThread =
      &worklet->workerBackingThread().backingThread();
  EXPECT_EQ(firstThread, secondThread);
  checkWorkletCanExecuteScript(worklet.get());

  worklet->terminateAndWait();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worklet is
// created while another is terminating.
TEST_F(AudioWorkletThreadTest, CreatingSecondDuringTerminationOfFirst) {
  std::unique_ptr<AudioWorkletThread> firstWorklet = createAudioWorkletThread();
  checkWorkletCanExecuteScript(firstWorklet.get());
  v8::Isolate* firstIsolate = firstWorklet->isolate();
  ASSERT_TRUE(firstIsolate);

  // Request termination of the first worklet and create the second worklet
  // as soon as possible. We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worklet thread so quickly. This could be a source of flakiness.
  firstWorklet->terminate();
  std::unique_ptr<AudioWorkletThread> secondWorklet =
      createAudioWorkletThread();

  v8::Isolate* secondIsolate = secondWorklet->isolate();
  ASSERT_TRUE(secondIsolate);
  EXPECT_EQ(firstIsolate, secondIsolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worklet.
  checkWorkletCanExecuteScript(secondWorklet.get());
  secondWorklet->terminateAndWait();
}

}  // namespace blink
