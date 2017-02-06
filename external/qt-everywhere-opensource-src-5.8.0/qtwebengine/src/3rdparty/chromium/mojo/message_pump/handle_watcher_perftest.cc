// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/test_support/test_support.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {

enum MessageLoopConfig {
  MESSAGE_LOOP_CONFIG_DEFAULT = 0,
  MESSAGE_LOOP_CONFIG_MOJO = 1
};

std::unique_ptr<base::MessageLoop> CreateMessageLoop(MessageLoopConfig config) {
  std::unique_ptr<base::MessageLoop> loop;
  if (config == MESSAGE_LOOP_CONFIG_DEFAULT)
    loop.reset(new base::MessageLoop());
  else
    loop.reset(new base::MessageLoop(MessagePumpMojo::Create()));
  return loop;
}

void OnWatcherSignaled(const base::Closure& callback, MojoResult /* result */) {
  callback.Run();
}

class ScopedPerfTimer {
 public:
  ScopedPerfTimer(const std::string& test_name,
                  const std::string& sub_test_name,
                  uint64_t iterations)
      : test_name_(test_name),
        sub_test_name_(sub_test_name),
        iterations_(iterations),
        start_time_(base::TimeTicks::Now()) {}
  ~ScopedPerfTimer() {
    base::TimeTicks end_time = base::TimeTicks::Now();
    mojo::test::LogPerfResult(
        test_name_.c_str(), sub_test_name_.c_str(),
        iterations_ / (end_time - start_time_).InSecondsF(),
        "iterations/second");
  }

 private:
  const std::string test_name_;
  const std::string sub_test_name_;
  const uint64_t iterations_;
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPerfTimer);
};

class HandleWatcherPerftest : public testing::TestWithParam<MessageLoopConfig> {
 public:
  HandleWatcherPerftest() : message_loop_(CreateMessageLoop(GetParam())) {}

 protected:
  std::string GetMessageLoopName() const {
    return (GetParam() == MESSAGE_LOOP_CONFIG_DEFAULT) ? "DefaultMessageLoop"
                                                       : "MojoMessageLoop";
  }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(HandleWatcherPerftest);
};

INSTANTIATE_TEST_CASE_P(MultipleMessageLoopConfigs,
                        HandleWatcherPerftest,
                        testing::Values(MESSAGE_LOOP_CONFIG_DEFAULT,
                                        MESSAGE_LOOP_CONFIG_MOJO));

void NeverReached(MojoResult result) {
  FAIL() << "Callback should never be invoked " << result;
}

TEST_P(HandleWatcherPerftest, StartStop) {
  const uint64_t kIterations = 100000;
  MessagePipe pipe;
  HandleWatcher watcher;

  ScopedPerfTimer timer("StartStop", GetMessageLoopName(), kIterations);
  for (uint64_t i = 0; i < kIterations; i++) {
    watcher.Start(pipe.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE,
                  MOJO_DEADLINE_INDEFINITE, base::Bind(&NeverReached));
    watcher.Stop();
  }
}

TEST_P(HandleWatcherPerftest, StartAllThenStop_1000Handles) {
  const uint64_t kIterations = 100;
  const uint64_t kHandles = 1000;

  struct TestData {
    MessagePipe pipe;
    HandleWatcher watcher;
  };
  ScopedVector<TestData> data_vector;
  // Create separately from the start/stop loops to avoid affecting the
  // benchmark.
  for (uint64_t i = 0; i < kHandles; i++) {
    std::unique_ptr<TestData> test_data(new TestData);
    ASSERT_TRUE(test_data->pipe.handle0.is_valid());
    data_vector.push_back(std::move(test_data));
  }

  ScopedPerfTimer timer("StartAllThenStop_1000Handles", GetMessageLoopName(),
                        kIterations * kHandles);
  for (uint64_t iter = 0; iter < kIterations; iter++) {
    for (uint64_t i = 0; i < kHandles; i++) {
      TestData* test_data = data_vector[i];
      test_data->watcher.Start(
          test_data->pipe.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE,
          MOJO_DEADLINE_INDEFINITE, base::Bind(&NeverReached));
    }
    for (uint64_t i = 0; i < kHandles; i++) {
      TestData* test_data = data_vector[i];
      test_data->watcher.Stop();
    }
  }
}

TEST_P(HandleWatcherPerftest, StartAndSignal) {
  const uint64_t kIterations = 10000;
  const std::string kMessage = "hello";
  MessagePipe pipe;
  HandleWatcher watcher;
  std::string received_message;

  ScopedPerfTimer timer("StartAndSignal", GetMessageLoopName(), kIterations);
  for (uint64_t i = 0; i < kIterations; i++) {
    base::RunLoop run_loop;
    watcher.Start(pipe.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE,
                  MOJO_DEADLINE_INDEFINITE,
                  base::Bind(&OnWatcherSignaled, run_loop.QuitClosure()));
    ASSERT_TRUE(mojo::test::WriteTextMessage(pipe.handle1.get(), kMessage));
    run_loop.Run();
    watcher.Stop();

    ASSERT_TRUE(
        mojo::test::ReadTextMessage(pipe.handle0.get(), &received_message));
    EXPECT_EQ(kMessage, received_message);
    received_message.clear();
  }
}

TEST_P(HandleWatcherPerftest, StartAndSignal_1000Waiting) {
  const uint64_t kIterations = 10000;
  const uint64_t kWaitingHandles = 1000;
  const std::string kMessage = "hello";
  MessagePipe pipe;
  HandleWatcher watcher;
  std::string received_message;

  struct TestData {
    MessagePipe pipe;
    HandleWatcher watcher;
  };
  ScopedVector<TestData> data_vector;
  for (uint64_t i = 0; i < kWaitingHandles; i++) {
    std::unique_ptr<TestData> test_data(new TestData);
    ASSERT_TRUE(test_data->pipe.handle0.is_valid());
    test_data->watcher.Start(
        test_data->pipe.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE,
        MOJO_DEADLINE_INDEFINITE, base::Bind(&NeverReached));
    data_vector.push_back(std::move(test_data));
  }

  ScopedPerfTimer timer("StartAndSignal_1000Waiting", GetMessageLoopName(),
                        kIterations);
  for (uint64_t i = 0; i < kIterations; i++) {
    base::RunLoop run_loop;
    watcher.Start(pipe.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE,
                  MOJO_DEADLINE_INDEFINITE,
                  base::Bind(&OnWatcherSignaled, run_loop.QuitClosure()));
    ASSERT_TRUE(mojo::test::WriteTextMessage(pipe.handle1.get(), kMessage));
    run_loop.Run();
    watcher.Stop();

    ASSERT_TRUE(
        mojo::test::ReadTextMessage(pipe.handle0.get(), &received_message));
    EXPECT_EQ(kMessage, received_message);
    received_message.clear();
  }
}

}  // namespace test
}  // namespace common
}  // namespace mojo
