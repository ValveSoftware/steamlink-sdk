// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_TESTS_BLIMP_BROWSER_TEST_H_
#define BLIMP_ENGINE_BROWSER_TESTS_BLIMP_BROWSER_TEST_H_

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test_base.h"

namespace base {
class RunLoop;
}

namespace content {
class WebContents;
}

namespace blimp {
namespace client {
struct Assignment;
}  // namespace client

namespace engine {
class BlimpEngineSession;
}  // namespace engine

// Base class for tests which require a full instance of the Blimp engine.
class BlimpBrowserTest : public content::BrowserTestBase {
 public:
  // Notify that an asynchronous test is now complete and the test runner should
  // exit.
  void QuitRunLoop();

  // Tells RunUntilCompletion() to break and discontinue processing UI & IO
  // thread MessageLoops.
  void SignalCompletion();

 protected:
  BlimpBrowserTest();
  ~BlimpBrowserTest() override;

  // Processes tasks in the UI and IO thread until SignalCompletion() is
  // called.
  void RunUntilCompletion();

  // Allow the UI thread to wait.  This bypasses base::ThreadRestrictions and is
  // used for client/engine integration tests.
  void AllowUIWaits();

  engine::BlimpEngineSession* GetEngineSession();

  client::Assignment GetAssignment();

  // testing::Test implementation.
  void SetUp() override;

  // content::BrowserTestBase implementation.
  void RunTestOnMainThreadLoop() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  // Receives the port number from an asynchronously connected socket.
  // Calls SignalCompletion() when set.
  void OnGetEnginePortCompletion(uint16_t port);

  uint16_t engine_port_;

  // Used to signal the completion of asynchronous processing to
  // RunUntilCompletion().
  base::WaitableEvent completion_event_;

  // Used to allow UI thread waits.  This is useful for integration tests that
  // require setting up client components, as testing those sometimes requires
  // setting up GL contexts that might block.
  std::unique_ptr<base::ThreadRestrictions::ScopedAllowWait> allow_ui_waits_;

  DISALLOW_COPY_AND_ASSIGN(BlimpBrowserTest);
};

}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_TESTS_BLIMP_BROWSER_TEST_H_
