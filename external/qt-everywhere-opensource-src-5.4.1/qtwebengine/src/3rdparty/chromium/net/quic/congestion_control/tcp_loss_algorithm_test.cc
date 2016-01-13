// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/rtt_stats.h"
#include "net/quic/congestion_control/tcp_loss_algorithm.h"
#include "net/quic/quic_unacked_packet_map.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class TcpLossAlgorithmTest : public ::testing::Test {
 protected:
  TcpLossAlgorithmTest()
      : unacked_packets_() {
    rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                         QuicTime::Delta::Zero(),
                         clock_.Now());
  }

  void SendDataPacket(QuicPacketSequenceNumber sequence_number) {
    SerializedPacket packet(sequence_number, PACKET_1BYTE_SEQUENCE_NUMBER,
                            NULL, 0, new RetransmittableFrames());
    unacked_packets_.AddPacket(packet);
    unacked_packets_.SetSent(sequence_number, clock_.Now(), 1000, true);
  }

  void VerifyLosses(QuicPacketSequenceNumber largest_observed,
                    QuicPacketSequenceNumber* losses_expected,
                    size_t num_losses) {
    SequenceNumberSet lost_packets =
        loss_algorithm_.DetectLostPackets(
            unacked_packets_, clock_.Now(), largest_observed, rtt_stats_);
    EXPECT_EQ(num_losses, lost_packets.size());
    for (size_t i = 0; i < num_losses; ++i) {
      EXPECT_TRUE(ContainsKey(lost_packets, losses_expected[i]));
    }
  }

  QuicUnackedPacketMap unacked_packets_;
  TCPLossAlgorithm loss_algorithm_;
  RttStats rtt_stats_;
  MockClock clock_;
};

TEST_F(TcpLossAlgorithmTest, NackRetransmit1Packet) {
  const size_t kNumSentPackets = 5;
  // Transmit 5 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  // No loss on one ack.
  unacked_packets_.RemoveFromInFlight(2);
  unacked_packets_.NackPacket(1, 1);
  VerifyLosses(2, NULL, 0);
  // No loss on two acks.
  unacked_packets_.RemoveFromInFlight(3);
  unacked_packets_.NackPacket(1, 2);
  VerifyLosses(3, NULL, 0);
  // Loss on three acks.
  unacked_packets_.RemoveFromInFlight(4);
  unacked_packets_.NackPacket(1, 3);
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(4, lost, arraysize(lost));
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

// A stretch ack is an ack that covers more than 1 packet of previously
// unacknowledged data.
TEST_F(TcpLossAlgorithmTest, NackRetransmit1PacketWith1StretchAck) {
  const size_t kNumSentPackets = 10;
  // Transmit 10 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  // Nack the first packet 3 times in a single StretchAck.
  unacked_packets_.NackPacket(1, 3);
  unacked_packets_.RemoveFromInFlight(2);
  unacked_packets_.RemoveFromInFlight(3);
  unacked_packets_.RemoveFromInFlight(4);
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(4, lost, arraysize(lost));
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

// Ack a packet 3 packets ahead, causing a retransmit.
TEST_F(TcpLossAlgorithmTest, NackRetransmit1PacketSingleAck) {
  const size_t kNumSentPackets = 10;
  // Transmit 10 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  // Nack the first packet 3 times in an AckFrame with three missing packets.
  unacked_packets_.NackPacket(1, 3);
  unacked_packets_.NackPacket(2, 2);
  unacked_packets_.NackPacket(3, 1);
  unacked_packets_.RemoveFromInFlight(4);
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(4, lost, arraysize(lost));
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

TEST_F(TcpLossAlgorithmTest, EarlyRetransmit1Packet) {
  const size_t kNumSentPackets = 2;
  // Transmit 2 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  // Early retransmit when the final packet gets acked and the first is nacked.
  unacked_packets_.RemoveFromInFlight(2);
  unacked_packets_.NackPacket(1, 1);
  VerifyLosses(2, NULL, 0);
  EXPECT_EQ(clock_.Now().Add(rtt_stats_.SmoothedRtt().Multiply(1.25)),
            loss_algorithm_.GetLossTimeout());

  clock_.AdvanceTime(rtt_stats_.latest_rtt().Multiply(1.25));
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(2, lost, arraysize(lost));
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

TEST_F(TcpLossAlgorithmTest, EarlyRetransmitAllPackets) {
  const size_t kNumSentPackets = 5;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
    // Advance the time 1/4 RTT between 3 and 4.
    if (i == 3) {
      clock_.AdvanceTime(rtt_stats_.SmoothedRtt().Multiply(0.25));
    }
  }

  // Early retransmit when the final packet gets acked and 1.25 RTTs have
  // elapsed since the packets were sent.
  unacked_packets_.RemoveFromInFlight(kNumSentPackets);
  // This simulates a single ack following multiple missing packets with FACK.
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    unacked_packets_.NackPacket(i, kNumSentPackets - i);
  }
  QuicPacketSequenceNumber lost[] = { 1, 2 };
  VerifyLosses(kNumSentPackets, lost, arraysize(lost));
  // The time has already advanced 1/4 an RTT, so ensure the timeout is set
  // 1.25 RTTs after the earliest pending packet(3), not the last(4).
  EXPECT_EQ(clock_.Now().Add(rtt_stats_.SmoothedRtt()),
            loss_algorithm_.GetLossTimeout());

  clock_.AdvanceTime(rtt_stats_.SmoothedRtt());
  QuicPacketSequenceNumber lost2[] = { 1, 2, 3 };
  VerifyLosses(kNumSentPackets, lost2, arraysize(lost2));
  EXPECT_EQ(clock_.Now().Add(rtt_stats_.SmoothedRtt().Multiply(0.25)),
            loss_algorithm_.GetLossTimeout());
  clock_.AdvanceTime(rtt_stats_.SmoothedRtt().Multiply(0.25));
  QuicPacketSequenceNumber lost3[] = { 1, 2, 3, 4 };
  VerifyLosses(kNumSentPackets, lost3, arraysize(lost3));
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

TEST_F(TcpLossAlgorithmTest, DontEarlyRetransmitNeuteredPacket) {
  const size_t kNumSentPackets = 2;
  // Transmit 2 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  // Neuter packet 1.
  unacked_packets_.RemoveRetransmittability(1);

  // Early retransmit when the final packet gets acked and the first is nacked.
  unacked_packets_.IncreaseLargestObserved(2);
  unacked_packets_.RemoveFromInFlight(2);
  unacked_packets_.NackPacket(1, 1);
  VerifyLosses(2, NULL, 0);
  EXPECT_EQ(QuicTime::Zero(), loss_algorithm_.GetLossTimeout());
}

}  // namespace test
}  // namespace net
