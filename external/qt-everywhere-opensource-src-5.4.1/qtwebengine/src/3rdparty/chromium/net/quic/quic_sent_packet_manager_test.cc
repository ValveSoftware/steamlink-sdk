// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/stl_util.h"
#include "net/quic/test_tools/quic_config_peer.h"
#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::vector;
using testing::ElementsAre;
using testing::Pair;
using testing::Pointwise;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace net {
namespace test {
namespace {

// Default packet length.
const uint32 kDefaultLength = 1000;

// Matcher to check the key of the key-value pair it receives as first argument
// equals its second argument.
MATCHER(KeyEq, "") {
  return std::tr1::get<0>(arg).first == std::tr1::get<1>(arg);
}

class MockDebugDelegate : public QuicSentPacketManager::DebugDelegate {
 public:
  MOCK_METHOD2(OnSpuriousPacketRetransmition,
               void(TransmissionType transmission_type,
                    QuicByteCount byte_size));
};

class QuicSentPacketManagerTest : public ::testing::TestWithParam<bool> {
 protected:
  QuicSentPacketManagerTest()
      : manager_(true, &clock_, &stats_, kFixRate, kNack),
        send_algorithm_(new StrictMock<MockSendAlgorithm>) {
    QuicSentPacketManagerPeer::SetSendAlgorithm(&manager_, send_algorithm_);
    // Disable tail loss probes for most tests.
    QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 0);
    // Advance the time 1s so the send times are never QuicTime::Zero.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));
  }

  virtual ~QuicSentPacketManagerTest() OVERRIDE {
    STLDeleteElements(&packets_);
  }

  QuicByteCount BytesInFlight() {
    return QuicSentPacketManagerPeer::GetBytesInFlight(&manager_);
  }
  void VerifyUnackedPackets(QuicPacketSequenceNumber* packets,
                            size_t num_packets) {
    if (num_packets == 0) {
      EXPECT_FALSE(manager_.HasUnackedPackets());
      EXPECT_EQ(0u, QuicSentPacketManagerPeer::GetNumRetransmittablePackets(
          &manager_));
      return;
    }

    EXPECT_TRUE(manager_.HasUnackedPackets());
    EXPECT_EQ(packets[0], manager_.GetLeastUnackedSentPacket());
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(manager_.IsUnacked(packets[i])) << packets[i];
    }
  }

  void VerifyRetransmittablePackets(QuicPacketSequenceNumber* packets,
                                    size_t num_packets) {
    EXPECT_EQ(num_packets,
              QuicSentPacketManagerPeer::GetNumRetransmittablePackets(
                  &manager_));
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(manager_.HasRetransmittableFrames(packets[i]))
          << " packets[" << i << "]:" << packets[i];
    }
  }

  void ExpectAck(QuicPacketSequenceNumber largest_observed) {
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(
        true, _, ElementsAre(Pair(largest_observed, _)), _));
  }

  void ExpectUpdatedRtt(QuicPacketSequenceNumber largest_observed) {
    EXPECT_CALL(*send_algorithm_,
                OnCongestionEvent(true, _, _, _));
  }

  void ExpectAckAndLoss(bool rtt_updated,
                        QuicPacketSequenceNumber largest_observed,
                        QuicPacketSequenceNumber lost_packet) {
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(
        rtt_updated, _, ElementsAre(Pair(largest_observed, _)),
        ElementsAre(Pair(lost_packet, _))));
  }

  // |packets_acked| and |packets_lost| should be in sequence number order.
  void ExpectAcksAndLosses(bool rtt_updated,
                           QuicPacketSequenceNumber* packets_acked,
                           size_t num_packets_acked,
                           QuicPacketSequenceNumber* packets_lost,
                           size_t num_packets_lost) {
    vector<QuicPacketSequenceNumber> ack_vector;
    for (size_t i = 0; i < num_packets_acked; ++i) {
      ack_vector.push_back(packets_acked[i]);
    }
    vector<QuicPacketSequenceNumber> lost_vector;
    for (size_t i = 0; i < num_packets_lost; ++i) {
      lost_vector.push_back(packets_lost[i]);
    }
    EXPECT_CALL(*send_algorithm_,
                OnCongestionEvent(rtt_updated, _,
                                  Pointwise(KeyEq(), ack_vector),
                                  Pointwise(KeyEq(), lost_vector)));
  }

  // Retransmits a packet as though it was a TLP retransmission, because TLP
  // leaves the |old_sequence_number| pending.
  // TODO(ianswett): Test with transmission types besides TLP.
  void RetransmitPacket(QuicPacketSequenceNumber old_sequence_number,
                        QuicPacketSequenceNumber new_sequence_number) {
    QuicSentPacketManagerPeer::MarkForRetransmission(
        &manager_, old_sequence_number, TLP_RETRANSMISSION);
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    QuicSentPacketManager::PendingRetransmission next_retransmission =
        manager_.NextPendingRetransmission();
    EXPECT_EQ(old_sequence_number, next_retransmission.sequence_number);
    EXPECT_EQ(TLP_RETRANSMISSION,
              next_retransmission.transmission_type);
    manager_.OnRetransmittedPacket(old_sequence_number,
                                   new_sequence_number);
    EXPECT_TRUE(QuicSentPacketManagerPeer::IsRetransmission(
        &manager_, new_sequence_number));
  }

  void RetransmitAndSendPacket(QuicPacketSequenceNumber old_sequence_number,
                               QuicPacketSequenceNumber new_sequence_number) {
    RetransmitPacket(old_sequence_number, new_sequence_number);

    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(), new_sequence_number,
                             kDefaultLength, HAS_RETRANSMITTABLE_DATA))
        .WillOnce(Return(true));
    manager_.OnPacketSent(new_sequence_number,
                          clock_.Now(),
                          kDefaultLength,
                          LOSS_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA);
  }

  SerializedPacket CreateDataPacket(QuicPacketSequenceNumber sequence_number) {
    return CreatePacket(sequence_number, true);
  }

  SerializedPacket CreatePacket(QuicPacketSequenceNumber sequence_number,
                                bool retransmittable) {
    packets_.push_back(QuicPacket::NewDataPacket(
        NULL, kDefaultLength, false, PACKET_8BYTE_CONNECTION_ID, false,
        PACKET_6BYTE_SEQUENCE_NUMBER));
    return SerializedPacket(
        sequence_number, PACKET_6BYTE_SEQUENCE_NUMBER,
        packets_.back(), 0u,
        retransmittable ? new RetransmittableFrames() : NULL);
  }

  SerializedPacket CreateFecPacket(QuicPacketSequenceNumber sequence_number) {
    packets_.push_back(QuicPacket::NewFecPacket(
        NULL, kDefaultLength, false, PACKET_8BYTE_CONNECTION_ID, false,
        PACKET_6BYTE_SEQUENCE_NUMBER));
    return SerializedPacket(sequence_number, PACKET_6BYTE_SEQUENCE_NUMBER,
                            packets_.back(), 0u, NULL);
  }

  void SendDataPacket(QuicPacketSequenceNumber sequence_number) {
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(), sequence_number, _, _))
                    .Times(1).WillOnce(Return(true));
    SerializedPacket packet(CreateDataPacket(sequence_number));
    manager_.OnSerializedPacket(packet);
    manager_.OnPacketSent(sequence_number, clock_.Now(),
                          packet.packet->length(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA);
  }

  void SendCryptoPacket(QuicPacketSequenceNumber sequence_number) {
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(), sequence_number,
                             kDefaultLength, HAS_RETRANSMITTABLE_DATA))
                    .Times(1).WillOnce(Return(true));
    SerializedPacket packet(CreateDataPacket(sequence_number));
    packet.retransmittable_frames->AddStreamFrame(
        new QuicStreamFrame(1, false, 0, IOVector()));
    packet.retransmittable_frames->set_encryption_level(ENCRYPTION_NONE);
    manager_.OnSerializedPacket(packet);
    manager_.OnPacketSent(sequence_number, clock_.ApproximateNow(),
                          packet.packet->length(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA);
  }

  void SendFecPacket(QuicPacketSequenceNumber sequence_number) {
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(), sequence_number,
                             kDefaultLength, NO_RETRANSMITTABLE_DATA))
                    .Times(1).WillOnce(Return(true));
    SerializedPacket packet(CreateFecPacket(sequence_number));
    manager_.OnSerializedPacket(packet);
    manager_.OnPacketSent(sequence_number, clock_.ApproximateNow(),
                          packet.packet->length(), NOT_RETRANSMISSION,
                          NO_RETRANSMITTABLE_DATA);
  }

  void SendAckPacket(QuicPacketSequenceNumber sequence_number) {
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(), sequence_number,
                             kDefaultLength, NO_RETRANSMITTABLE_DATA))
                    .Times(1).WillOnce(Return(false));
    SerializedPacket packet(CreatePacket(sequence_number, false));
    manager_.OnSerializedPacket(packet);
    manager_.OnPacketSent(sequence_number, clock_.Now(),
                          packet.packet->length(), NOT_RETRANSMISSION,
                          NO_RETRANSMITTABLE_DATA);
  }

  // Based on QuicConnection's WritePendingRetransmissions.
  void RetransmitNextPacket(
      QuicPacketSequenceNumber retransmission_sequence_number) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, _, retransmission_sequence_number,
                             kDefaultLength, HAS_RETRANSMITTABLE_DATA))
                    .Times(1).WillOnce(Return(true));
    const QuicSentPacketManager::PendingRetransmission pending =
        manager_.NextPendingRetransmission();
    manager_.OnRetransmittedPacket(pending.sequence_number,
                                   retransmission_sequence_number);
    manager_.OnPacketSent(retransmission_sequence_number, clock_.Now(),
                          kDefaultLength, pending.transmission_type,
                          HAS_RETRANSMITTABLE_DATA);
  }

  QuicSentPacketManager manager_;
  vector<QuicPacket*> packets_;
  MockClock clock_;
  QuicConnectionStats stats_;
  MockSendAlgorithm* send_algorithm_;
};

TEST_F(QuicSentPacketManagerTest, IsUnacked) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreateDataPacket(1));

  manager_.OnSerializedPacket(serialized_packet);

  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 1 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, IsUnAckedRetransmit) {
  SendDataPacket(1);
  RetransmitPacket(1, 2);

  EXPECT_TRUE(QuicSentPacketManagerPeer::IsRetransmission(&manager_, 2));
  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 2 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAck) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Ack 2 but not 1.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  ExpectAck(2);
  manager_.OnIncomingAck(received_info, clock_.Now());

  // Packet 1 is unacked, pending, but not retransmittable.
  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckBeforeSend) {
  SendDataPacket(1);
  QuicSentPacketManagerPeer::MarkForRetransmission(
      &manager_, 1, TLP_RETRANSMISSION);
  EXPECT_TRUE(manager_.HasPendingRetransmissions());

  // Ack 1.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  ExpectAck(1);
  manager_.OnIncomingAck(received_info, clock_.Now());

  // There should no longer be a pending retransmission.
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // No unacked packets remain.
  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
  EXPECT_EQ(0u, stats_.packets_spuriously_retransmitted);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckPrevious) {
  SendDataPacket(1);
  RetransmitPacket(1, 2);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // Ack 1 but not 2.
  ExpectAck(1);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  // 2 should be unacked, since it may provide an RTT measurement.
  QuicPacketSequenceNumber unacked[] = { 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  VerifyRetransmittablePackets(NULL, 0);

  // Verify that the retransmission alarm would not fire,
  // since there is no retransmittable data outstanding.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
  EXPECT_EQ(1u, stats_.packets_spuriously_retransmitted);
}

TEST_F(QuicSentPacketManagerTest, RetransmitAndSendThenAckPrevious) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // Ack 1 but not 2.
  ExpectAck(1);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  // 2 remains unacked, but no packets have retransmittable data.
  QuicPacketSequenceNumber unacked[] = { 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  VerifyRetransmittablePackets(NULL, 0);

  EXPECT_EQ(1u, stats_.packets_spuriously_retransmitted);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckPreviousThenNackRetransmit) {
  SendDataPacket(1);
  RetransmitPacket(1, 2);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2, _, _))
      .WillOnce(Return(true));
  manager_.OnPacketSent(2, clock_.ApproximateNow(), kDefaultLength,
                        LOSS_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // First, ACK packet 1 which makes packet 2 non-retransmittable.
  ExpectAck(1);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  SendDataPacket(3);
  SendDataPacket(4);
  SendDataPacket(5);
  clock_.AdvanceTime(rtt);

  // Next, NACK packet 2 three times.
  received_info.largest_observed = 3;
  received_info.missing_packets.insert(2);
  ExpectAck(3);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  received_info.largest_observed = 4;
  ExpectAck(4);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  received_info.largest_observed = 5;
  ExpectAckAndLoss(true, 5, 2);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  // No packets remain unacked.
  VerifyUnackedPackets(NULL, 0);
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  VerifyRetransmittablePackets(NULL, 0);

  // Verify that the retransmission alarm would not fire,
  // since there is no retransmittable data outstanding.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, RetransmitTwiceThenAckPreviousBeforeSend) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Fire the RTO, which will mark 2 for retransmission (but will not send it).
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  manager_.OnRetransmissionTimeout();
  EXPECT_TRUE(manager_.HasPendingRetransmissions());

  // Ack 1 but not 2, before 2 is able to be sent.
  // Since 1 has been retransmitted, it has already been lost, and so the
  // send algorithm is not informed that it has been ACK'd.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  ExpectUpdatedRtt(1);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  // Since 2 was marked for retransmit, when 1 is acked, 2 is kept for RTT.
  QuicPacketSequenceNumber unacked[] = { 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  VerifyRetransmittablePackets(NULL, 0);

  // Verify that the retransmission alarm would not fire,
  // since there is no retransmittable data outstanding.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, RetransmitTwiceThenAckFirst) {
  StrictMock<MockDebugDelegate> debug_delegate;
  EXPECT_CALL(debug_delegate, OnSpuriousPacketRetransmition(
      TLP_RETRANSMISSION, kDefaultLength)).Times(2);
  manager_.set_debug_delegate(&debug_delegate);

  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  RetransmitAndSendPacket(2, 3);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // Ack 1 but not 2 or 3.
  ExpectAck(1);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  // 2 and 3 remain unacked, but no packets have retransmittable data.
  QuicPacketSequenceNumber unacked[] = { 2, 3 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  VerifyRetransmittablePackets(NULL, 0);

  // Ensure packet 2 is lost when 4 is sent and 3 and 4 are acked.
  SendDataPacket(4);
  received_info.largest_observed = 4;
  received_info.missing_packets.insert(2);
  QuicPacketSequenceNumber acked[] = { 3, 4 };
  ExpectAcksAndLosses(true, acked, arraysize(acked), NULL, 0);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  QuicPacketSequenceNumber unacked2[] = { 2 };
  VerifyUnackedPackets(unacked2, arraysize(unacked2));
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));

  SendDataPacket(5);
  received_info.largest_observed = 5;
  ExpectAckAndLoss(true, 5, 2);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  VerifyUnackedPackets(NULL, 0);
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  EXPECT_EQ(2u, stats_.packets_spuriously_retransmitted);
}

TEST_F(QuicSentPacketManagerTest, LoseButDontRetransmitRevivedPacket) {
  SendDataPacket(1);
  SendDataPacket(2);
  SendFecPacket(3);
  SendDataPacket(4);

  // Ack 2 and 3, and mark 1 as revived.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 3;
  received_info.missing_packets.insert(1);
  received_info.revived_packets.insert(1);
  QuicPacketSequenceNumber acked[] = { 2, 3 };
  ExpectAcksAndLosses(true, acked, arraysize(acked), NULL, 0);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  QuicPacketSequenceNumber unacked[] = { 1, 4 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  QuicPacketSequenceNumber retransmittable[] = { 4 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));

  // Ack the 4th packet and expect the 1st to be considered lost.
  received_info.largest_observed = 4;
  ExpectAckAndLoss(true, 4, 1);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicSentPacketManagerTest, MarkLostThenReviveAndDontRetransmitPacket) {
  SendDataPacket(1);
  SendDataPacket(2);
  SendDataPacket(3);
  SendDataPacket(4);
  SendFecPacket(5);

  // Ack 2, 3, and 4, and expect the 1st to be considered lost.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 4;
  received_info.missing_packets.insert(1);
  QuicPacketSequenceNumber acked[] = { 2, 3, 4 };
  QuicPacketSequenceNumber lost[] = { 1 };
  ExpectAcksAndLosses(true, acked, arraysize(acked), lost, arraysize(lost));
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  QuicPacketSequenceNumber unacked[] = { 1, 5 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 1 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));

  // Ack 5th packet (FEC) and revive 1st packet. 1st packet should now be
  // removed from pending retransmissions map.
  received_info.largest_observed = 5;
  received_info.revived_packets.insert(1);
  ExpectAck(5);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicSentPacketManagerTest, TruncatedAck) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  RetransmitAndSendPacket(2, 3);
  RetransmitAndSendPacket(3, 4);
  RetransmitAndSendPacket(4, 5);

  // Truncated ack with 4 NACKs, so the first packet is lost.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 4;
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  received_info.missing_packets.insert(3);
  received_info.missing_packets.insert(4);
  received_info.is_truncated = true;

  QuicPacketSequenceNumber lost[] = { 1 };
  ExpectAcksAndLosses(true, NULL, 0, lost, arraysize(lost));
  manager_.OnIncomingAck(received_info, clock_.Now());

  // High water mark will be raised.
  QuicPacketSequenceNumber unacked[] = { 2, 3, 4, 5 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 5 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, AckPreviousTransmissionThenTruncatedAck) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  RetransmitAndSendPacket(2, 3);
  RetransmitAndSendPacket(3, 4);
  manager_.OnSerializedPacket(CreateDataPacket(5));
  manager_.OnSerializedPacket(CreateDataPacket(6));
  manager_.OnSerializedPacket(CreateDataPacket(7));
  manager_.OnSerializedPacket(CreateDataPacket(8));
  manager_.OnSerializedPacket(CreateDataPacket(9));

  // Ack previous transmission
  {
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 2;
    received_info.missing_packets.insert(1);
    ExpectAck(2);
    manager_.OnIncomingAck(received_info, clock_.Now());
    EXPECT_TRUE(manager_.IsUnacked(4));
  }

  // Truncated ack with 4 NACKs
  {
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 6;
    received_info.missing_packets.insert(3);
    received_info.missing_packets.insert(4);
    received_info.missing_packets.insert(5);
    received_info.missing_packets.insert(6);
    received_info.is_truncated = true;
    ExpectAckAndLoss(false, 1, 3);
    manager_.OnIncomingAck(received_info, clock_.Now());
  }

  // High water mark will be raised.
  QuicPacketSequenceNumber unacked[] = { 4, 5, 6, 7, 8, 9 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 5, 6, 7, 8, 9 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacket) {
  EXPECT_EQ(0u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnacked) {
  SerializedPacket serialized_packet(CreateDataPacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnackedFec) {
  SerializedPacket serialized_packet(CreateFecPacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedPacketAndDiscard) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreateFecPacket(1));
  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());

  SerializedPacket serialized_packet2(CreateFecPacket(2));
  manager_.OnSerializedPacket(serialized_packet2);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());

  SerializedPacket serialized_packet3(CreateFecPacket(3));
  manager_.OnSerializedPacket(serialized_packet3);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());

  QuicPacketSequenceNumber unacked[] = { 1, 2, 3 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  // Ack 2, which has never been sent, so there's no rtt update.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  manager_.OnIncomingAck(received_info, clock_.Now());

  EXPECT_EQ(3u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetSentTime) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreateFecPacket(1));
  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _))
                  .Times(1).WillOnce(Return(true));
  manager_.OnPacketSent(1, QuicTime::Zero(), kDefaultLength, NOT_RETRANSMISSION,
                        NO_RETRANSMITTABLE_DATA);

  SerializedPacket serialized_packet2(CreateFecPacket(2));
  QuicTime sent_time = QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(1));
  manager_.OnSerializedPacket(serialized_packet2);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2, _, _))
                  .Times(1).WillOnce(Return(true));
  manager_.OnPacketSent(2, sent_time, kDefaultLength, NOT_RETRANSMISSION,
                        NO_RETRANSMITTABLE_DATA);

  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  EXPECT_TRUE(manager_.HasUnackedPackets());
  EXPECT_EQ(QuicTime::Zero(),
            QuicSentPacketManagerPeer::GetSentTime(&manager_, 1));
  EXPECT_EQ(sent_time, QuicSentPacketManagerPeer::GetSentTime(&manager_, 2));
}

TEST_F(QuicSentPacketManagerTest, AckAckAndUpdateRtt) {
  SendDataPacket(1);
  SendAckPacket(2);

  // Now ack the ack and expect an RTT update.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);

  ExpectAck(1);
  manager_.OnIncomingAck(received_info, clock_.Now());

  SendAckPacket(3);

  // Now ack the ack and expect only an RTT update.
  received_info.largest_observed = 3;
  ExpectUpdatedRtt(3);
  manager_.OnIncomingAck(received_info, clock_.Now());
}

TEST_F(QuicSentPacketManagerTest, Rtt) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(15);
  SendDataPacket(sequence_number);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

  ExpectAck(sequence_number);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  manager_.OnIncomingAck(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt,
            QuicSentPacketManagerPeer::GetRttStats(&manager_)->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, RttWithInvalidDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // delta_time_largest_observed is larger than the local time elapsed
  // and is hence invalid.
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(sequence_number);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(sequence_number);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(11);
  manager_.OnIncomingAck(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt,
            QuicSentPacketManagerPeer::GetRttStats(&manager_)->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, RttWithInfiniteDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // delta_time_largest_observed is infinite, and is hence invalid.
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(sequence_number);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(sequence_number);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed = QuicTime::Delta::Infinite();
  manager_.OnIncomingAck(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt,
            QuicSentPacketManagerPeer::GetRttStats(&manager_)->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, RttZeroDelta) {
  // Expect that the RTT is the time between send and receive since the
  // delta_time_largest_observed is zero.
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(sequence_number);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(sequence_number);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed = QuicTime::Delta::Zero();
  manager_.OnIncomingAck(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt,
            QuicSentPacketManagerPeer::GetRttStats(&manager_)->latest_rtt());
}

TEST_F(QuicSentPacketManagerTest, TailLossProbeTimeout) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 2);

  // Send 1 packet.
  QuicPacketSequenceNumber sequence_number = 1;
  SendDataPacket(sequence_number);

  // The first tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  manager_.MaybeRetransmitTailLossProbe();
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  RetransmitNextPacket(2);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // The second tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  manager_.MaybeRetransmitTailLossProbe();
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  RetransmitNextPacket(3);
  EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).WillOnce(Return(
      QuicTime::Delta::Infinite()));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // Ack the third and ensure the first two are still pending.
  ExpectAck(3);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 3;
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));

  // Acking two more packets will lose both of them due to nacks.
  received_info.largest_observed = 5;
  QuicPacketSequenceNumber lost[] = { 1, 2 };
  ExpectAcksAndLosses(false, NULL, 0, lost, arraysize(lost));
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
  EXPECT_EQ(2u, stats_.tlp_count);
  EXPECT_EQ(0u, stats_.rto_count);
}

TEST_F(QuicSentPacketManagerTest, TailLossProbeThenRTO) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 2);

  // Send 100 packets.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  // The first tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  manager_.MaybeRetransmitTailLossProbe();
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  RetransmitNextPacket(101);
  EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).WillOnce(Return(
      QuicTime::Delta::Infinite()));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // The second tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_TRUE(manager_.MaybeRetransmitTailLossProbe());
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  RetransmitNextPacket(102);
  EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).WillOnce(Return(
      QuicTime::Delta::Infinite()));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));

  // Advance the time enough to ensure all packets are RTO'd.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));

  // The final RTO abandons all of them.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  manager_.OnRetransmissionTimeout();
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  EXPECT_EQ(2u, stats_.tlp_count);
  EXPECT_EQ(1u, stats_.rto_count);
}

TEST_F(QuicSentPacketManagerTest, CryptoHandshakeTimeout) {
  // Send 2 crypto packets and 3 data packets.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  const size_t kNumSentDataPackets = 3;
  for (size_t i = 1; i <= kNumSentDataPackets; ++i) {
    SendDataPacket(kNumSentCryptoPackets + i);
  }
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // The first retransmits 2 packets.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(6);
  RetransmitNextPacket(7);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // The second retransmits 2 packets.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(8);
  RetransmitNextPacket(9);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // Now ack the two crypto packets and the speculatively encrypted request,
  // and ensure the first four crypto packets get abandoned, but not lost.
  QuicPacketSequenceNumber acked[] = { 3, 4, 5, 8, 9 };
  ExpectAcksAndLosses(true, acked, arraysize(acked), NULL, 0);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 9;
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  received_info.missing_packets.insert(6);
  received_info.missing_packets.insert(7);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_FALSE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));
}

TEST_F(QuicSentPacketManagerTest, CryptoHandshakeTimeoutVersionNegotiation) {
  // Send 2 crypto packets and 3 data packets.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  const size_t kNumSentDataPackets = 3;
  for (size_t i = 1; i <= kNumSentDataPackets; ++i) {
    SendDataPacket(kNumSentCryptoPackets + i);
  }
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // The first retransmission timeout retransmits 2 crypto packets.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(6);
  RetransmitNextPacket(7);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // Now act like a version negotiation packet arrived, which would cause all
  // unacked packets to be retransmitted.
  manager_.RetransmitUnackedPackets(ALL_PACKETS);

  // Ensure the first two pending packets are the crypto retransmits.
  ASSERT_TRUE(manager_.HasPendingRetransmissions());
  EXPECT_EQ(6u, manager_.NextPendingRetransmission().sequence_number);
  RetransmitNextPacket(8);
  EXPECT_EQ(7u, manager_.NextPendingRetransmission().sequence_number);
  RetransmitNextPacket(9);

  EXPECT_TRUE(manager_.HasPendingRetransmissions());
}

TEST_F(QuicSentPacketManagerTest, CryptoHandshakeSpuriousRetransmission) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // Retransmit the crypto packet as 2.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(2);

  // Retransmit the crypto packet as 3.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(3);

  // Now ack the second crypto packet, and ensure the first gets removed, but
  // the third does not.
  ExpectUpdatedRtt(2);
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  EXPECT_FALSE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));
  QuicPacketSequenceNumber unacked[] = { 3 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
}

TEST_F(QuicSentPacketManagerTest, CryptoHandshakeTimeoutUnsentDataPacket) {
  // Send 2 crypto packets and serialize 1 data packet.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  SerializedPacket packet(CreateDataPacket(3));
  manager_.OnSerializedPacket(packet);
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // Retransmit 2 crypto packets, but not the serialized packet.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(6);
  RetransmitNextPacket(7);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));
}

TEST_F(QuicSentPacketManagerTest,
       CryptoHandshakeRetransmissionThenRetransmitAll) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // Retransmit the crypto packet as 2.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(2);

  // Now retransmit all the unacked packets, which occurs when there is a
  // version negotiation.
  manager_.RetransmitUnackedPackets(ALL_PACKETS);
  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
}

TEST_F(QuicSentPacketManagerTest,
       CryptoHandshakeRetransmissionThenNeuterAndAck) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));

  // Retransmit the crypto packet as 2.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(2);

  // Retransmit the crypto packet as 3.
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(3);

  // Now neuter all unacked unencrypted packets, which occurs when the
  // connection goes forward secure.
  manager_.NeuterUnencryptedPackets();
  QuicPacketSequenceNumber unacked[] = { 1, 2, 3};
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));

  // Ensure both packets get discarded when packet 2 is acked.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 3;
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  ExpectUpdatedRtt(3);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());
  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicSentPacketManagerTest, TailLossProbeTimeoutUnsentDataPacket) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 2);
  // Serialize two data packets and send the latter.
  SerializedPacket packet(CreateDataPacket(1));
  manager_.OnSerializedPacket(packet);
  SendDataPacket(2);
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));

  // Retransmit 1 unacked packets, but not the first serialized packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  manager_.MaybeRetransmitTailLossProbe();
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  RetransmitNextPacket(3);
  EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).WillOnce(Return(
      QuicTime::Delta::Infinite()));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_FALSE(QuicSentPacketManagerPeer::HasUnackedCryptoPackets(&manager_));
  EXPECT_TRUE(QuicSentPacketManagerPeer::HasPendingPackets(&manager_));
}

TEST_F(QuicSentPacketManagerTest, ResetRecentMinRTTWithEmptyWindow) {
  QuicTime::Delta min_rtt = QuicTime::Delta::FromMilliseconds(50);
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->UpdateRtt(
      min_rtt, QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_EQ(min_rtt,
            QuicSentPacketManagerPeer::GetRttStats(&manager_)->min_rtt());
  EXPECT_EQ(min_rtt,
            QuicSentPacketManagerPeer::GetRttStats(
                &manager_)->recent_min_rtt());

  // Send two packets with no prior bytes in flight.
  SendDataPacket(1);
  SendDataPacket(2);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  // Ack two packets with 100ms RTT observations.
  ReceivedPacketInfo received_info;
  received_info.delta_time_largest_observed = QuicTime::Delta::Zero();
  received_info.largest_observed = 1;
  ExpectAck(1);
  manager_.OnIncomingAck(received_info, clock_.Now());

  // First ack does not change recent min rtt.
  EXPECT_EQ(min_rtt,
            QuicSentPacketManagerPeer::GetRttStats(
                &manager_)->recent_min_rtt());

  received_info.largest_observed = 2;
  ExpectAck(2);
  manager_.OnIncomingAck(received_info, clock_.Now());

  EXPECT_EQ(min_rtt,
            QuicSentPacketManagerPeer::GetRttStats(&manager_)->min_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100),
            QuicSentPacketManagerPeer::GetRttStats(
                &manager_)->recent_min_rtt());
}

TEST_F(QuicSentPacketManagerTest, RetransmissionTimeout) {
  // Send 100 packets and then ensure all are abandoned when the RTO fires.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_FALSE(manager_.MaybeRetransmitTailLossProbe());
  manager_.OnRetransmissionTimeout();
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionTime) {
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionTimeCryptoHandshake) {
  SendCryptoPacket(1);

  // Check the min.
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->set_initial_rtt_us(
      1 * base::Time::kMicrosecondsPerMillisecond);
  EXPECT_EQ(clock_.Now().Add(QuicTime::Delta::FromMilliseconds(10)),
            manager_.GetRetransmissionTime());

  // Test with a standard smoothed RTT.
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->set_initial_rtt_us(
      100 * base::Time::kMicrosecondsPerMillisecond);

  QuicTime::Delta srtt = manager_.GetRttStats()->SmoothedRtt();
  QuicTime expected_time = clock_.Now().Add(srtt.Multiply(1.5));
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(srtt.Multiply(1.5));
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(2);

  // The retransmission time should now be twice as far in the future.
  expected_time = clock_.Now().Add(srtt.Multiply(2).Multiply(1.5));
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionTimeTailLossProbe) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 2);
  SendDataPacket(1);
  SendDataPacket(2);

  // Check the min.
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->set_initial_rtt_us(
      1 * base::Time::kMicrosecondsPerMillisecond);
  EXPECT_EQ(clock_.Now().Add(QuicTime::Delta::FromMilliseconds(10)),
            manager_.GetRetransmissionTime());

  // Test with a standard smoothed RTT.
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->set_initial_rtt_us(
      100 * base::Time::kMicrosecondsPerMillisecond);
  QuicTime::Delta srtt = manager_.GetRttStats()->SmoothedRtt();
  QuicTime::Delta expected_tlp_delay = srtt.Multiply(2);
  QuicTime expected_time = clock_.Now().Add(expected_tlp_delay);
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(expected_tlp_delay);
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_TRUE(manager_.MaybeRetransmitTailLossProbe());
  EXPECT_TRUE(manager_.HasPendingRetransmissions());
  RetransmitNextPacket(3);
  EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).WillOnce(Return(
      QuicTime::Delta::Infinite()));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            manager_.TimeUntilSend(clock_.Now(), HAS_RETRANSMITTABLE_DATA));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  expected_time = clock_.Now().Add(expected_tlp_delay);
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionTimeRTO) {
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->UpdateRtt(
      QuicTime::Delta::FromMilliseconds(100),
      QuicTime::Delta::Zero(),
      QuicTime::Zero());

  SendDataPacket(1);
  SendDataPacket(2);

  QuicTime::Delta expected_rto_delay = QuicTime::Delta::FromMilliseconds(500);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillRepeatedly(Return(expected_rto_delay));
  QuicTime expected_time = clock_.Now().Add(expected_rto_delay);
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  clock_.AdvanceTime(expected_rto_delay);
  manager_.OnRetransmissionTimeout();
  RetransmitNextPacket(3);
  RetransmitNextPacket(4);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // The delay should double the second time.
  expected_time = clock_.Now().Add(expected_rto_delay).Add(expected_rto_delay);
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Ack a packet and ensure the RTO goes back to the original value.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  ExpectUpdatedRtt(2);
  manager_.OnIncomingAck(received_info, clock_.ApproximateNow());

  expected_time = clock_.Now().Add(expected_rto_delay);
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionDelayMin) {
  SendDataPacket(1);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillRepeatedly(Return(QuicTime::Delta::FromMilliseconds(1)));
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(200);

  // If the delay is smaller than the min, ensure it exponentially backs off
  // from the min.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay,
              QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
    delay = delay.Add(delay);
    EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
    manager_.OnRetransmissionTimeout();
    RetransmitNextPacket(i + 2);
  }
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionDelayMax) {
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(QuicTime::Delta::FromSeconds(500)));

  EXPECT_EQ(QuicTime::Delta::FromSeconds(60),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionDelay) {
  SendDataPacket(1);
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(500);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillRepeatedly(Return(delay));

  // Delay should back off exponentially.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay,
              QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
    delay = delay.Add(delay);
    EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
    manager_.OnRetransmissionTimeout();
    RetransmitNextPacket(i + 2);
  }
}

TEST_F(QuicSentPacketManagerTest, GetLossDelay) {
  MockLossAlgorithm* loss_algorithm = new MockLossAlgorithm();
  QuicSentPacketManagerPeer::SetLossAlgorithm(&manager_, loss_algorithm);

  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillRepeatedly(Return(QuicTime::Zero()));
  SendDataPacket(1);
  SendDataPacket(2);

  // Handle an ack which causes the loss algorithm to be evaluated and
  // set the loss timeout.
  ExpectAck(2);
  EXPECT_CALL(*loss_algorithm, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(SequenceNumberSet()));
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  manager_.OnIncomingAck(received_info, clock_.Now());

  QuicTime timeout(clock_.Now().Add(QuicTime::Delta::FromMilliseconds(10)));
  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillRepeatedly(Return(timeout));
  EXPECT_EQ(timeout, manager_.GetRetransmissionTime());

  // Fire the retransmission timeout and ensure the loss detection algorithm
  // is invoked.
  EXPECT_CALL(*loss_algorithm, DetectLostPackets(_, _, _, _))
      .WillOnce(Return(SequenceNumberSet()));
  manager_.OnRetransmissionTimeout();
}

TEST_F(QuicSentPacketManagerTest, NegotiateTimeLossDetection) {
  EXPECT_EQ(kNack,
            QuicSentPacketManagerPeer::GetLossAlgorithm(
                &manager_)->GetLossDetectionType());

  QuicConfig config;
  QuicConfigPeer::SetReceivedLossDetection(&config, kTIME);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);

  EXPECT_EQ(kTime,
            QuicSentPacketManagerPeer::GetLossAlgorithm(
                &manager_)->GetLossDetectionType());
}


}  // namespace
}  // namespace test
}  // namespace net
