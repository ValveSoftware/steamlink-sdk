// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_output_buffer.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeArgument;
using testing::Ref;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {

class BlimpMessageOutputBufferTest : public testing::Test {
 public:
  BlimpMessageOutputBufferTest() {}

  void SetUp() override {
    input_msg_.mutable_input();
    input_msg_.set_message_id(1);
    compositor_msg_.mutable_compositor();
    compositor_msg_.set_message_id(2);

    // Buffer should only have space for two unacknowledged messages
    // (with message IDs).
    ASSERT_EQ(input_msg_.ByteSize(), compositor_msg_.ByteSize());
    buffer_.reset(new BlimpMessageOutputBuffer(2 * input_msg_.GetCachedSize()));
  }

 protected:
  void AddOutputExpectation(const BlimpMessage& msg) {
    EXPECT_CALL(output_processor_, MockableProcessMessage(EqualsProto(msg), _))
        .WillOnce(SaveArg<1>(&captured_cb_))
        .RetiresOnSaturation();
  }

  BlimpMessage WithMessageId(const BlimpMessage& message, int64_t message_id) {
    BlimpMessage output = message;
    output.set_message_id(message_id);
    return output;
  }

  BlimpMessage input_msg_;
  BlimpMessage compositor_msg_;

  base::MessageLoop message_loop_;
  net::CompletionCallback captured_cb_;
  MockBlimpMessageProcessor output_processor_;
  std::unique_ptr<BlimpMessageOutputBuffer> buffer_;
  testing::InSequence s;
};

// Verify batched writes and acknowledgements.
TEST_F(BlimpMessageOutputBufferTest, SeparatelyBufferWriteAck) {
  net::TestCompletionCallback complete_cb_1;
  net::TestCompletionCallback complete_cb_2;

  AddOutputExpectation(input_msg_);
  AddOutputExpectation(compositor_msg_);

  // Accumulate two messages.
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(input_msg_)),
                          complete_cb_1.callback());
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(compositor_msg_)),
                          complete_cb_2.callback());
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());

  // Write two messages.
  ASSERT_TRUE(captured_cb_.is_null());
  buffer_->SetOutputProcessor(&output_processor_);
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(2, buffer_->GetUnacknowledgedMessageCountForTest());

  // Both messages are acknowledged by separate checkpoints.
  buffer_->OnMessageCheckpoint(1);
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());
  buffer_->OnMessageCheckpoint(2);
  ASSERT_EQ(0, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());
  EXPECT_EQ(net::OK, complete_cb_2.WaitForResult());
}

// Verify buffer writes from an empty state.
TEST_F(BlimpMessageOutputBufferTest, WritesFromEmptyBuffer) {
  net::TestCompletionCallback complete_cb_1;
  net::TestCompletionCallback complete_cb_2;

  AddOutputExpectation(input_msg_);
  AddOutputExpectation(compositor_msg_);

  ASSERT_TRUE(captured_cb_.is_null());
  buffer_->SetOutputProcessor(&output_processor_);

  // Message #0 is buffered, sent, acknowledged.
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(input_msg_)),
                          complete_cb_1.callback());
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());
  buffer_->OnMessageCheckpoint(1);
  ASSERT_EQ(0, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());

  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(compositor_msg_)),
                          complete_cb_2.callback());
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());
  buffer_->OnMessageCheckpoint(2);
  ASSERT_EQ(0, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());
}

// Verify that a single checkpoint can be used to acknowledge two writes.
TEST_F(BlimpMessageOutputBufferTest, SharedCheckpoint) {
  net::TestCompletionCallback complete_cb_1;
  net::TestCompletionCallback complete_cb_2;

  AddOutputExpectation(input_msg_);
  AddOutputExpectation(compositor_msg_);

  // Message #1 is written but unacknowledged.
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(input_msg_)),
                          complete_cb_1.callback());
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_TRUE(captured_cb_.is_null());
  buffer_->SetOutputProcessor(&output_processor_);
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());

  // Message #2 is written but unacknowledged.
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(compositor_msg_)),
                          complete_cb_2.callback());
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_TRUE(captured_cb_.is_null());
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(2, buffer_->GetUnacknowledgedMessageCountForTest());

  // Both messages are acknowledged in one checkpoint.
  buffer_->OnMessageCheckpoint(2);
  ASSERT_EQ(0, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());
  EXPECT_EQ(net::OK, complete_cb_2.WaitForResult());
}

// Verify that messages that fail to write are kept in a pending write state.
TEST_F(BlimpMessageOutputBufferTest, WriteError) {
  net::TestCompletionCallback complete_cb_1;
  net::TestCompletionCallback complete_cb_2;

  AddOutputExpectation(input_msg_);
  AddOutputExpectation(input_msg_);

  // Accumulate two messages.
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(input_msg_)),
                          complete_cb_1.callback());
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());

  // First write attempt, which fails.
  ASSERT_TRUE(captured_cb_.is_null());
  buffer_->SetOutputProcessor(&output_processor_);
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::ERR_FAILED);
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());

  // Simulate disconnect.
  buffer_->SetOutputProcessor(nullptr);

  // Reconnect. Should immediately try to write the contents of the buffer.
  buffer_->SetOutputProcessor(&output_processor_);
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());
  buffer_->OnMessageCheckpoint(1);
  ASSERT_EQ(0, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());
}

// Verify that unacknowledged messages can be moved back to a pending write
// state (recovery after a lost connection.)
TEST_F(BlimpMessageOutputBufferTest, MessageRetransmit) {
  net::TestCompletionCallback complete_cb_1;
  net::TestCompletionCallback complete_cb_2;

  AddOutputExpectation(input_msg_);
  AddOutputExpectation(compositor_msg_);
  AddOutputExpectation(compositor_msg_);  // Retransmitted message.

  // Accumulate two messages.
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(input_msg_)),
                          complete_cb_1.callback());
  buffer_->ProcessMessage(base::WrapUnique(new BlimpMessage(compositor_msg_)),
                          complete_cb_2.callback());
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());

  // Write two messages.
  ASSERT_TRUE(captured_cb_.is_null());
  buffer_->SetOutputProcessor(&output_processor_);
  ASSERT_FALSE(captured_cb_.is_null());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(2, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(2, buffer_->GetUnacknowledgedMessageCountForTest());

  // Simulate disconnect & reconnect.
  buffer_->SetOutputProcessor(nullptr);
  buffer_->SetOutputProcessor(&output_processor_);

  // Remote end indicates that it only received message #0.
  // Message #1 should be moved from an unacknowledged state to a pending write
  // state.
  ASSERT_TRUE(captured_cb_.is_null());
  buffer_->OnMessageCheckpoint(1);
  buffer_->RetransmitBufferedMessages();
  ASSERT_FALSE(captured_cb_.is_null());
  ASSERT_EQ(1, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());
  base::ResetAndReturn(&captured_cb_).Run(net::OK);
  ASSERT_EQ(1, buffer_->GetUnacknowledgedMessageCountForTest());

  // Remote end acknowledges #1, buffer should be empty.
  buffer_->OnMessageCheckpoint(2);
  ASSERT_EQ(0, buffer_->GetBufferByteSizeForTest());
  ASSERT_EQ(0, buffer_->GetUnacknowledgedMessageCountForTest());
  EXPECT_EQ(net::OK, complete_cb_2.WaitForResult());
}

}  // namespace
}  // namespace blimp
