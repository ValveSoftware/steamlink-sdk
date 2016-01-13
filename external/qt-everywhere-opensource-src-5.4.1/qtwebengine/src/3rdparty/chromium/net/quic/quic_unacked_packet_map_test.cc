// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_unacked_packet_map.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

// Default packet length.
const uint32 kDefaultAckLength = 50;
const uint32 kDefaultLength = 1000;

class QuicUnackedPacketMapTest : public ::testing::Test {
 protected:
  QuicUnackedPacketMapTest()
      : now_(QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(1000))) {
  }

  SerializedPacket CreateRetransmittablePacket(
      QuicPacketSequenceNumber sequence_number) {
    return SerializedPacket(sequence_number, PACKET_1BYTE_SEQUENCE_NUMBER, NULL,
                            0, new RetransmittableFrames());
  }

  SerializedPacket CreateNonRetransmittablePacket(
      QuicPacketSequenceNumber sequence_number) {
    return SerializedPacket(
        sequence_number, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL);
  }

  void VerifyPendingPackets(QuicPacketSequenceNumber* packets,
                            size_t num_packets) {
    if (num_packets == 0) {
      EXPECT_FALSE(unacked_packets_.HasInFlightPackets());
      EXPECT_FALSE(unacked_packets_.HasMultipleInFlightPackets());
      return;
    }
    if (num_packets == 1) {
      EXPECT_TRUE(unacked_packets_.HasInFlightPackets());
      EXPECT_FALSE(unacked_packets_.HasMultipleInFlightPackets());
    }
    for (size_t i = 0; i < num_packets; ++i) {
      ASSERT_TRUE(unacked_packets_.IsUnacked(packets[i]));
      EXPECT_TRUE(unacked_packets_.GetTransmissionInfo(packets[i]).in_flight);
    }
  }

  void VerifyUnackedPackets(QuicPacketSequenceNumber* packets,
                            size_t num_packets) {
    if (num_packets == 0) {
      EXPECT_FALSE(unacked_packets_.HasUnackedPackets());
      EXPECT_FALSE(unacked_packets_.HasUnackedRetransmittableFrames());
      return;
    }
    EXPECT_TRUE(unacked_packets_.HasUnackedPackets());
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(unacked_packets_.IsUnacked(packets[i])) << packets[i];
    }
  }

  void VerifyRetransmittablePackets(QuicPacketSequenceNumber* packets,
                                    size_t num_packets) {
    size_t num_retransmittable_packets = 0;
    for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
         it != unacked_packets_.end(); ++it) {
      if (it->second.retransmittable_frames != NULL) {
        ++num_retransmittable_packets;
      }
    }
    EXPECT_EQ(num_packets, num_retransmittable_packets);
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(unacked_packets_.HasRetransmittableFrames(packets[i]))
          << " packets[" << i << "]:" << packets[i];
    }
  }

  QuicUnackedPacketMap unacked_packets_;
  QuicTime now_;
};

TEST_F(QuicUnackedPacketMapTest, RttOnly) {
  // Acks are only tracked for RTT measurement purposes.
  unacked_packets_.AddPacket(CreateNonRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultAckLength, false);

  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyPendingPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.IncreaseLargestObserved(1);
  VerifyUnackedPackets(NULL, 0);
  VerifyPendingPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicUnackedPacketMapTest, RetransmittableInflightAndRtt) {
  // Simulate a retransmittable packet being sent and acked.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyPendingPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(unacked, arraysize(unacked));

  unacked_packets_.RemoveRetransmittability(1);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyPendingPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.IncreaseLargestObserved(1);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyPendingPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.RemoveFromInFlight(1);
  VerifyUnackedPackets(NULL, 0);
  VerifyPendingPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicUnackedPacketMapTest, RetransmittedPacket) {
  // Simulate a retransmittable packet being sent, retransmitted, and the first
  // transmission being acked.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);
  unacked_packets_.OnRetransmittedPacket(1, 2, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(2, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyPendingPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 2 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));

  unacked_packets_.RemoveRetransmittability(1);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyPendingPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.IncreaseLargestObserved(2);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyPendingPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.RemoveFromInFlight(2);
  QuicPacketSequenceNumber unacked2[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked2));
  VerifyPendingPackets(unacked, arraysize(unacked2));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.RemoveFromInFlight(1);
  VerifyUnackedPackets(NULL, 0);
  VerifyPendingPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

}  // namespace
}  // namespace test
}  // namespace net
