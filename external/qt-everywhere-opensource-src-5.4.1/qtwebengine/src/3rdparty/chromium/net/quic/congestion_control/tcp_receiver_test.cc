// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/tcp_receiver.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class QuicTcpReceiverTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    receiver_.reset(new TcpReceiver());
  }
  scoped_ptr<TcpReceiver> receiver_;
};

TEST_F(QuicTcpReceiverTest, SimpleReceiver) {
  QuicCongestionFeedbackFrame feedback;
  QuicTime timestamp(QuicTime::Zero());
  receiver_->RecordIncomingPacket(1, 1, timestamp);
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  EXPECT_EQ(kTCP, feedback.type);
  EXPECT_EQ(256000u, feedback.tcp.receive_window);
  receiver_->RecordIncomingPacket(1, 2, timestamp);
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  EXPECT_EQ(kTCP, feedback.type);
  EXPECT_EQ(256000u, feedback.tcp.receive_window);
}

}  // namespace test
}  // namespace net
