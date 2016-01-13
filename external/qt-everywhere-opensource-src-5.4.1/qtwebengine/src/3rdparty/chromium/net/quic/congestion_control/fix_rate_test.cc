// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for FixRate sender and receiver.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/fix_rate_receiver.h"
#include "net/quic/congestion_control/fix_rate_sender.h"
#include "net/quic/congestion_control/rtt_stats.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

// bytes_in_flight is unused by FixRateSender's OnPacketSent.
QuicByteCount kUnused = 0;

class FixRateReceiverPeer : public FixRateReceiver {
 public:
  FixRateReceiverPeer()
      : FixRateReceiver() {
  }
  void SetBitrate(QuicBandwidth fix_rate) {
    FixRateReceiver::configured_rate_ = fix_rate;
  }
};

class FixRateTest : public ::testing::Test {
 protected:
  FixRateTest()
      : sender_(new FixRateSender(&rtt_stats_)),
        receiver_(new FixRateReceiverPeer()),
        start_(clock_.Now()) {
    // Make sure clock does not start at 0.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  }
  RttStats rtt_stats_;
  MockClock clock_;
  scoped_ptr<FixRateSender> sender_;
  scoped_ptr<FixRateReceiverPeer> receiver_;
  const QuicTime start_;
};

TEST_F(FixRateTest, ReceiverAPI) {
  QuicCongestionFeedbackFrame feedback;
  QuicTime timestamp(QuicTime::Zero());
  receiver_->SetBitrate(QuicBandwidth::FromKBytesPerSecond(300));
  receiver_->RecordIncomingPacket(1, 1, timestamp);
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  EXPECT_EQ(kFixRate, feedback.type);
  EXPECT_EQ(300000u, feedback.fix_rate.bitrate.ToBytesPerSecond());
}

TEST_F(FixRateTest, SenderAPI) {
  QuicCongestionFeedbackFrame feedback;
  feedback.type = kFixRate;
  feedback.fix_rate.bitrate = QuicBandwidth::FromKBytesPerSecond(300);
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback,  clock_.Now());
  EXPECT_EQ(300000, sender_->BandwidthEstimate().ToBytesPerSecond());
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
                                     0,
                                     HAS_RETRANSMITTABLE_DATA).IsZero());

  sender_->OnPacketSent(clock_.Now(), kUnused, 1, kDefaultMaxPacketSize,
                        HAS_RETRANSMITTABLE_DATA);
  EXPECT_FALSE(sender_->TimeUntilSend(clock_.Now(),
                                      kDefaultMaxPacketSize,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
  clock_.AdvanceTime(sender_->TimeUntilSend(clock_.Now(),
                                            kDefaultMaxPacketSize,
                                            HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
                                     kDefaultMaxPacketSize,
                                     HAS_RETRANSMITTABLE_DATA).IsZero());
  sender_->OnPacketSent(clock_.Now(), kUnused, 2, kDefaultMaxPacketSize,
                        HAS_RETRANSMITTABLE_DATA);
  EXPECT_FALSE(sender_->TimeUntilSend(clock_.Now(),
                                      kDefaultMaxPacketSize,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
  // Advance the time twice as much and expect only one packet to be sent.
  clock_.AdvanceTime(sender_->TimeUntilSend(
      clock_.Now(),
      kDefaultMaxPacketSize,
      HAS_RETRANSMITTABLE_DATA).Multiply(2));
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
                                     kDefaultMaxPacketSize,
                                     HAS_RETRANSMITTABLE_DATA).IsZero());
  sender_->OnPacketSent(clock_.Now(), kUnused, 3, kDefaultMaxPacketSize,
                        HAS_RETRANSMITTABLE_DATA);
  EXPECT_FALSE(sender_->TimeUntilSend(clock_.Now(),
                                      kDefaultMaxPacketSize,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
}

}  // namespace test
}  // namespace net
