// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/standalone/service_helper.h"

#include <signal.h>
#include <unistd.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ServiceHelperTest : public ::testing::Test {
 public:
  ServiceHelperTest() {}
  ~ServiceHelperTest() override {}

  void Quit() {
    CHECK(base_loop_);
    CHECK(run_loop_);
    base_loop_->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  void Init() {
    // Dynamically create the message loop to avoid thread check failure of task
    // runner in Debug mode.
    base_loop_.reset(new base::MessageLoopForIO());
    run_loop_.reset(new base::RunLoop());

    // This cannot be put inside SetUp() because we need to run it after fork().
    helper_.reset(new ServiceHelper());
    helper_->Init(base::Bind(&ServiceHelperTest::Quit,
                            base::Unretained(this)));
  }

 protected:
  std::unique_ptr<base::MessageLoopForIO> base_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ServiceHelper> helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceHelperTest);
};

TEST_F(ServiceHelperTest, CheckExternalTerm) {
  // Fork ourselves and run it.
  pid_t child_pid = fork();
  if (child_pid == 0) {  // Child process
    Init();
    run_loop_->Run();
    helper_.reset();  // Make sure ServiceHelper dtor does not crash.
    _Exit(EXIT_SUCCESS);
  } else if (child_pid == -1) {  // Failed to fork
    FAIL();
  } else {  // Parent process
    // Deliberate race. If ServiceHelper had bugs and stalled, the short
    // timeout would kill forked process and stop the test. In that case
    // there will be a crash from improperly terminated message loop
    // and we shall capture the error.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
    kill(child_pid, SIGTERM);
    int status;
    ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
  }
}

}  // namespace arc

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
