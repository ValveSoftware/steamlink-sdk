// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_checkpointer.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/net/blimp_message_checkpoint_observer.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace blimp {

namespace {
class MockCheckpointObserver : public BlimpMessageCheckpointObserver {
 public:
  MockCheckpointObserver() {}
  ~MockCheckpointObserver() override {}

  MOCK_METHOD1(OnMessageCheckpoint, void(int64_t));
};

}  // namespace

class BlimpMessageCheckpointerTest : public testing::Test {
 public:
  BlimpMessageCheckpointerTest()
      : runner_(new base::TestMockTimeTaskRunner), runner_handle_(runner_) {}

  ~BlimpMessageCheckpointerTest() override {}

  int64_t SimulateIncomingMessage() {
    InputMessage* input = nullptr;
    std::unique_ptr<BlimpMessage> message(CreateBlimpMessage(&input));
    message->set_message_id(++message_id_);
    checkpointer_->ProcessMessage(
        std::move(message),
        base::Bind(&BlimpMessageCheckpointerTest::IncomingCompletionCallback,
                   base::Unretained(this)));
    return message_id_;
  }

  void SetUp() override {
    checkpointer_ = base::WrapUnique(new BlimpMessageCheckpointer(
        &incoming_processor_, &outgoing_processor_, &checkpoint_observer_));
  }

  MOCK_METHOD1(IncomingCompletionCallback, void(int));

 protected:
  // Used to verify timing-specific behaviours.
  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handle_;

  int64_t message_id_ = 0;

  testing::StrictMock<MockBlimpMessageProcessor> incoming_processor_;
  testing::StrictMock<MockBlimpMessageProcessor> outgoing_processor_;
  testing::StrictMock<MockCheckpointObserver> checkpoint_observer_;
  net::CompletionCallback captured_cb_;

  std::unique_ptr<BlimpMessageCheckpointer> checkpointer_;
};

TEST_F(BlimpMessageCheckpointerTest, CallbackPropagates) {
  EXPECT_CALL(incoming_processor_, MockableProcessMessage(_, _))
      .WillOnce(SaveArg<1>(&captured_cb_));
  EXPECT_CALL(*this, IncomingCompletionCallback(_));

  // Simulate an incoming message.
  SimulateIncomingMessage();

  // Verify TestCompletionCallback called on completion.
  captured_cb_.Run(net::OK);
}

TEST_F(BlimpMessageCheckpointerTest, DeleteWhileProcessing) {
  EXPECT_CALL(incoming_processor_, MockableProcessMessage(_, _))
      .WillOnce(SaveArg<1>(&captured_cb_));
  EXPECT_CALL(*this, IncomingCompletionCallback(_)).Times(0);

  // Simulate an incoming message.
  SimulateIncomingMessage();

  // Delete the checkpointer, then simulate completion of processing.
  // TestCompletionCallback should not fire, so no expectations.
  checkpointer_ = nullptr;
  captured_cb_.Run(net::OK);
}

TEST_F(BlimpMessageCheckpointerTest, SingleMessageAck) {
  EXPECT_CALL(incoming_processor_, MockableProcessMessage(_, _))
      .WillOnce(SaveArg<1>(&captured_cb_));
  std::unique_ptr<BlimpMessage> expected_ack = CreateCheckpointAckMessage(1);
  EXPECT_CALL(outgoing_processor_,
              MockableProcessMessage(EqualsProto(*expected_ack), _));
  EXPECT_CALL(*this, IncomingCompletionCallback(net::OK));

  // Simulate an incoming message.
  SimulateIncomingMessage();

  // Fast-forward time to verify that an ACK message is sent.
  captured_cb_.Run(net::OK);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
}

TEST_F(BlimpMessageCheckpointerTest, BatchMessageAck) {
  EXPECT_CALL(incoming_processor_, MockableProcessMessage(_, _))
      .Times(10)
      .WillRepeatedly(SaveArg<1>(&captured_cb_));
  std::unique_ptr<BlimpMessage> expected_ack = CreateCheckpointAckMessage(10);
  EXPECT_CALL(outgoing_processor_,
              MockableProcessMessage(EqualsProto(*expected_ack), _));
  EXPECT_CALL(*this, IncomingCompletionCallback(net::OK)).Times(10);

  // Simulate ten incoming messages.
  for (int i = 0; i < 10; ++i) {
    SimulateIncomingMessage();
    captured_cb_.Run(net::OK);
  }

  // Fast-forward time to verify that only a single ACK message is sent.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
}

TEST_F(BlimpMessageCheckpointerTest, MultipleAcks) {
  EXPECT_CALL(incoming_processor_, MockableProcessMessage(_, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&captured_cb_));
  std::unique_ptr<BlimpMessage> expected_ack1 = CreateCheckpointAckMessage(1);
  EXPECT_CALL(outgoing_processor_,
              MockableProcessMessage(EqualsProto(*expected_ack1), _));
  std::unique_ptr<BlimpMessage> expected_ack2 = CreateCheckpointAckMessage(2);
  EXPECT_CALL(outgoing_processor_,
              MockableProcessMessage(EqualsProto(*expected_ack2), _));
  EXPECT_CALL(*this, IncomingCompletionCallback(net::OK)).Times(2);

  // Simulate an incoming messages and fast-forward to get the ACK.
  SimulateIncomingMessage();
  captured_cb_.Run(net::OK);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));

  // And do it again to verify we get a fresh ACK.
  SimulateIncomingMessage();
  captured_cb_.Run(net::OK);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
}

TEST_F(BlimpMessageCheckpointerTest, IncomingAckMessage) {
  EXPECT_CALL(*this, IncomingCompletionCallback(net::OK));
  EXPECT_CALL(checkpoint_observer_, OnMessageCheckpoint(10));

  // Simulate an incoming message.
  std::unique_ptr<BlimpMessage> ack_message = CreateCheckpointAckMessage(10);
  checkpointer_->ProcessMessage(
      std::move(ack_message),
      base::Bind(&BlimpMessageCheckpointerTest::IncomingCompletionCallback,
                 base::Unretained(this)));
}

}  // namespace blimp
