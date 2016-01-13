// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace net {
namespace test {
namespace {

class QuicAckNotifierTest : public ::testing::Test {
 protected:
  QuicAckNotifierTest() : zero_(QuicTime::Delta::Zero()) {}

  virtual void SetUp() {
    delegate_ = new MockAckNotifierDelegate;
    notifier_.reset(new QuicAckNotifier(delegate_));

    notifier_->AddSequenceNumber(26, 100);
    notifier_->AddSequenceNumber(99, 20);
    notifier_->AddSequenceNumber(1234, 3);
  }

  MockAckNotifierDelegate* delegate_;
  scoped_ptr<QuicAckNotifier> notifier_;
  QuicTime::Delta zero_;
};

// Should trigger callback when we receive acks for all the registered seqnums.
TEST_F(QuicAckNotifierTest, TriggerCallback) {
  EXPECT_CALL(*delegate_, OnAckNotification(3, 123, 0, 0, zero_)).Times(1);
  EXPECT_FALSE(notifier_->OnAck(26, zero_));
  EXPECT_FALSE(notifier_->OnAck(99, zero_));
  EXPECT_TRUE(notifier_->OnAck(1234, zero_));
}

// Should not trigger callback if we never provide all the seqnums.
TEST_F(QuicAckNotifierTest, DoesNotTrigger) {
  // Should not trigger callback as not all packets have been seen.
  EXPECT_CALL(*delegate_, OnAckNotification(_, _, _, _, _)).Times(0);
  EXPECT_FALSE(notifier_->OnAck(26, zero_));
  EXPECT_FALSE(notifier_->OnAck(99, zero_));
}

// Should trigger even after updating sequence numbers and receiving ACKs for
// new sequeunce numbers.
TEST_F(QuicAckNotifierTest, UpdateSeqNums) {
  // Update a couple of the sequence numbers (i.e. retransmitted packets)
  notifier_->UpdateSequenceNumber(99, 3000);
  notifier_->UpdateSequenceNumber(1234, 3001);

  EXPECT_CALL(*delegate_, OnAckNotification(3, 123, 2, 20 + 3, _)).Times(1);
  EXPECT_FALSE(notifier_->OnAck(26, zero_));    // original
  EXPECT_FALSE(notifier_->OnAck(3000, zero_));  // updated
  EXPECT_TRUE(notifier_->OnAck(3001, zero_));   // updated
}

// Make sure the delegate is called with the delta time from the last ACK.
TEST_F(QuicAckNotifierTest, DeltaTime) {
  const QuicTime::Delta first_delta = QuicTime::Delta::FromSeconds(5);
  const QuicTime::Delta second_delta = QuicTime::Delta::FromSeconds(33);
  const QuicTime::Delta third_delta = QuicTime::Delta::FromSeconds(10);

  EXPECT_CALL(*delegate_,
              OnAckNotification(3, 123, 0, 0, third_delta))
      .Times(1);
  EXPECT_FALSE(notifier_->OnAck(26, first_delta));
  EXPECT_FALSE(notifier_->OnAck(99, second_delta));
  EXPECT_TRUE(notifier_->OnAck(1234, third_delta));
}

}  // namespace
}  // namespace test
}  // namespace net
