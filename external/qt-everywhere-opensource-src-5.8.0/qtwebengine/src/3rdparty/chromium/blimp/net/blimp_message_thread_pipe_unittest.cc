// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_thread_pipe.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "blimp/net/null_blimp_message_processor.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace blimp {

class BlimpMessageThreadPipeTest : public testing::Test {
 public:
  BlimpMessageThreadPipeTest() : thread_("PipeThread") {}

  ~BlimpMessageThreadPipeTest() override {}

  void SetUp() override {
    // Start the target processor thread and initialize the pipe & proxy.
    // Note that none of this will "touch" the target processor, so it's
    // safe to do here, before EXPECT_CALL() expectations are set up.
    ASSERT_TRUE(thread_.Start());
    pipe_ = base::WrapUnique(new BlimpMessageThreadPipe(thread_.task_runner()));
    proxy_ = pipe_->CreateProxy();

    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&BlimpMessageThreadPipe::set_target_processor,
                              base::Unretained(pipe_.get()), &null_processor_));
  }

  void TearDown() override {
    // If |pipe_| is still active, tear it down safely on |thread_|.
    if (pipe_)
      DeletePipeOnThread();

    // Synchronize with |thread_| to ensure that any pending work is done.
    SynchronizeWithThread();
  }

  MOCK_METHOD1(MockCompletionCallback, void(int));

  void DeletePipeOnThread() {
    thread_.task_runner()->DeleteSoon(FROM_HERE, pipe_.release());
  }

  void SynchronizeWithThread() {
    net::TestCompletionCallback cb;
    thread_.task_runner()->PostTaskAndReply(FROM_HERE,
                                            base::Bind(&base::DoNothing),
                                            base::Bind(cb.callback(), net::OK));
    ASSERT_EQ(net::OK, cb.WaitForResult());
  }

 protected:
  base::MessageLoop message_loop_;

  NullBlimpMessageProcessor null_processor_;

  std::unique_ptr<BlimpMessageThreadPipe> pipe_;
  std::unique_ptr<BlimpMessageProcessor> proxy_;

  base::Thread thread_;
};

TEST_F(BlimpMessageThreadPipeTest, ProcessMessage) {
  EXPECT_CALL(*this, MockCompletionCallback(_)).Times(1);

  // Pass a message to the proxy for processing.
  proxy_->ProcessMessage(
      base::WrapUnique(new BlimpMessage),
      base::Bind(&BlimpMessageThreadPipeTest::MockCompletionCallback,
                 base::Unretained(this)));
}

TEST_F(BlimpMessageThreadPipeTest, DeleteProxyBeforeCompletion) {
  EXPECT_CALL(*this, MockCompletionCallback(_)).Times(0);

  // Pass a message to the proxy, but then immediately delete the proxy.
  proxy_->ProcessMessage(
      base::WrapUnique(new BlimpMessage),
      base::Bind(&BlimpMessageThreadPipeTest::MockCompletionCallback,
                 base::Unretained(this)));
  proxy_ = nullptr;
}

TEST_F(BlimpMessageThreadPipeTest, DeletePipeBeforeProcessMessage) {
  EXPECT_CALL(*this, MockCompletionCallback(_)).Times(1);

  // Tear down the pipe (on |thread_|) between two ProcessMessage calls.
  proxy_->ProcessMessage(
      base::WrapUnique(new BlimpMessage),
      base::Bind(&BlimpMessageThreadPipeTest::MockCompletionCallback,
                 base::Unretained(this)));
  DeletePipeOnThread();
  proxy_->ProcessMessage(
      base::WrapUnique(new BlimpMessage),
      base::Bind(&BlimpMessageThreadPipeTest::MockCompletionCallback,
                 base::Unretained(this)));
}

TEST_F(BlimpMessageThreadPipeTest, NullCompletionCallback) {
  // Don't expect the mock to be called, but do expect not to crash.
  EXPECT_CALL(*this, MockCompletionCallback(_)).Times(0);

  proxy_->ProcessMessage(base::WrapUnique(new BlimpMessage),
                         net::CompletionCallback());
}

}  // namespace blimp
