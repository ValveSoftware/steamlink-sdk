// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/thread_pipe_manager.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/net/blimp_message_thread_pipe.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace blimp {
namespace {

// A feature that registers itself with ThreadPipeManager.
class FakeFeature {
 public:
  FakeFeature(BlimpMessage::FeatureCase feature_case,
              ThreadPipeManager* pipe_manager_) {
    outgoing_message_processor_ = pipe_manager_->RegisterFeature(
        feature_case, &incoming_message_processor_);
  }

  ~FakeFeature() {}

  BlimpMessageProcessor* outgoing_message_processor() {
    return outgoing_message_processor_.get();
  }

  MockBlimpMessageProcessor* incoming_message_processor() {
    return &incoming_message_processor_;
  }

 private:
  testing::StrictMock<MockBlimpMessageProcessor> incoming_message_processor_;
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor_;
};

// A feature peer on |thread_| that forwards incoming messages to
// |message_processor|.
class FakeFeaturePeer : public BlimpMessageProcessor {
 public:
  FakeFeaturePeer(BlimpMessage::FeatureCase feature_case,
                  BlimpMessageProcessor* message_processor,
                  const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : feature_case_(feature_case),
        message_processor_(message_processor),
        task_runner_(task_runner) {}

  ~FakeFeaturePeer() override {}

 private:
  void ForwardMessage(std::unique_ptr<BlimpMessage> message) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    message_processor_->ProcessMessage(std::move(message),
                                       net::CompletionCallback());
  }

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    ASSERT_EQ(feature_case_, message->feature_case());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&FakeFeaturePeer::ForwardMessage,
                              base::Unretained(this), base::Passed(&message)));
    if (!callback.is_null())
      callback.Run(net::OK);
  }

  BlimpMessage::FeatureCase feature_case_;
  BlimpMessageProcessor* message_processor_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// A browser connection handler that returns FakeFeaturePeer to allow it
// forwarding message back so that FakeFeature can check message it receives
// with one it just sent.
class FakeBrowserConnectionHandler : public BrowserConnectionHandler {
 public:
  FakeBrowserConnectionHandler(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : task_runner_(task_runner) {}
  std::unique_ptr<BlimpMessageProcessor> RegisterFeature(
      BlimpMessage::FeatureCase feature_case,
      BlimpMessageProcessor* incoming_processor) override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    return base::WrapUnique(
        new FakeFeaturePeer(feature_case, incoming_processor, task_runner_));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

class ThreadPipeManagerTest : public testing::Test {
 public:
  ThreadPipeManagerTest() : thread_("IoThread") {}

  ~ThreadPipeManagerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(thread_.Start());
    connection_handler_ = base::WrapUnique(
        new FakeBrowserConnectionHandler(thread_.task_runner()));
    pipe_manager_ = base::WrapUnique(new ThreadPipeManager(
        thread_.task_runner(), connection_handler_.get()));

    input_feature_.reset(
        new FakeFeature(BlimpMessage::kInput, pipe_manager_.get()));
    tab_control_feature_.reset(
        new FakeFeature(BlimpMessage::kTabControl, pipe_manager_.get()));
  }

  void TearDown() override { SynchronizeWithThread(); }

  // Synchronize with |thread_| to ensure that any pending work is done.
  void SynchronizeWithThread() {
    net::TestCompletionCallback cb;
    thread_.task_runner()->PostTaskAndReply(FROM_HERE,
                                            base::Bind(&base::DoNothing),
                                            base::Bind(cb.callback(), net::OK));
    ASSERT_EQ(net::OK, cb.WaitForResult());
  }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<BrowserConnectionHandler> connection_handler_;
  std::unique_ptr<ThreadPipeManager> pipe_manager_;
  base::Thread thread_;

  std::unique_ptr<FakeFeature> input_feature_;
  std::unique_ptr<FakeFeature> tab_control_feature_;
};

// Features send out message and receive the same message due to
// |FakeFeaturePeer| loops the message back on |thread_|.
TEST_F(ThreadPipeManagerTest, MessageSentIsReceived) {
  InputMessage* input = nullptr;
  std::unique_ptr<BlimpMessage> input_message = CreateBlimpMessage(&input);
  TabControlMessage* tab_control = nullptr;
  std::unique_ptr<BlimpMessage> tab_control_message =
      CreateBlimpMessage(&tab_control);

  EXPECT_CALL(*(input_feature_->incoming_message_processor()),
              MockableProcessMessage(EqualsProto(*input_message), _))
      .RetiresOnSaturation();
  EXPECT_CALL(*(tab_control_feature_->incoming_message_processor()),
              MockableProcessMessage(EqualsProto(*tab_control_message), _))
      .RetiresOnSaturation();

  net::TestCompletionCallback cb1;
  input_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(input_message), cb1.callback());
  net::TestCompletionCallback cb2;
  tab_control_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(tab_control_message), cb2.callback());

  EXPECT_EQ(net::OK, cb1.WaitForResult());
  EXPECT_EQ(net::OK, cb2.WaitForResult());
}

}  // namespace blimp
