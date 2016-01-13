// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_base.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/test_service/test_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
namespace test {
namespace {

typedef ShellTestBase ShellTestBaseTest;

class QuitMessageLoopErrorHandler : public ErrorHandler {
 public:
  QuitMessageLoopErrorHandler() {}
  virtual ~QuitMessageLoopErrorHandler() {}

  // |ErrorHandler| implementation:
  virtual void OnConnectionError() OVERRIDE {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitMessageLoopErrorHandler);
};

void PingCallback(base::MessageLoop* message_loop, bool* was_run) {
  *was_run = true;
  VLOG(2) << "Ping callback";
  message_loop->QuitWhenIdle();
}

TEST_F(ShellTestBaseTest, LaunchServiceInProcess) {
  InitMojo();

  InterfacePtr<mojo::test::ITestService> test_service;

  {
    MessagePipe mp;
    test_service.Bind(mp.handle0.Pass());
    LaunchServiceInProcess(GURL("mojo:mojo_test_service"),
                           mojo::test::ITestService::Name_,
                           mp.handle1.Pass());
  }

  bool was_run = false;
  test_service->Ping(base::Bind(&PingCallback,
                                base::Unretained(message_loop()),
                                base::Unretained(&was_run)));
  message_loop()->Run();
  EXPECT_TRUE(was_run);
  EXPECT_FALSE(test_service.encountered_error());

  test_service.reset();

  // This will run until the test service has actually quit (which it will,
  // since we killed the only connection to it).
  message_loop()->Run();
}

// Tests that launching a service in process fails properly if the service
// doesn't exist.
TEST_F(ShellTestBaseTest, LaunchServiceInProcessInvalidService) {
  InitMojo();

  InterfacePtr<mojo::test::ITestService> test_service;

  {
    MessagePipe mp;
    test_service.Bind(mp.handle0.Pass());
    LaunchServiceInProcess(GURL("mojo:non_existent_service"),
                           mojo::test::ITestService::Name_,
                           mp.handle1.Pass());
  }

  bool was_run = false;
  test_service->Ping(base::Bind(&PingCallback,
                                base::Unretained(message_loop()),
                                base::Unretained(&was_run)));

  // This will quit because there's nothing running.
  message_loop()->Run();
  EXPECT_FALSE(was_run);

  // It may have quit before an error was processed.
  if (!test_service.encountered_error()) {
    QuitMessageLoopErrorHandler quitter;
    test_service.set_error_handler(&quitter);
    message_loop()->Run();
    EXPECT_TRUE(test_service.encountered_error());
  }

  test_service.reset();
}

}  // namespace
}  // namespace test
}  // namespace shell
}  // namespace mojo
