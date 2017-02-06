// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_TESTS_BLIMP_BROWSER_TEST_H_
#define BLIMP_ENGINE_BROWSER_TESTS_BLIMP_BROWSER_TEST_H_

#include <memory>

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

 protected:
  BlimpBrowserTest();
  ~BlimpBrowserTest() override;

  // Run an asynchronous test in a nested run loop. The caller should call
  // QuitRunLoop() to notify that the test should finish.
  void RunUntilQuit();

  engine::BlimpEngineSession* GetEngineSession();

  client::Assignment GetAssignment();

  // testing::Test implementation.
  void SetUp() override;

  // content::BrowserTestBase implementation.
  void RunTestOnMainThreadLoop() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  void OnGetEnginePort(uint16_t port);

  uint16_t engine_port_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BlimpBrowserTest);
};

}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_TESTS_BLIMP_BROWSER_TEST_H_
