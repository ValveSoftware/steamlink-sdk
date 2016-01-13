// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_

#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"

namespace net {

class SendAlgorithmInterface;

namespace test {

class QuicSentPacketManagerPeer {
 public:
  static void SetMaxTailLossProbes(
      QuicSentPacketManager* sent_packet_manager, size_t max_tail_loss_probes);

  static void SetSendAlgorithm(QuicSentPacketManager* sent_packet_manager,
                               SendAlgorithmInterface* send_algorithm);

  static const LossDetectionInterface* GetLossAlgorithm(
      QuicSentPacketManager* sent_packet_manager);

  static void SetLossAlgorithm(QuicSentPacketManager* sent_packet_manager,
                               LossDetectionInterface* loss_detector);

  static RttStats* GetRttStats(QuicSentPacketManager* sent_packet_manager);

  static size_t GetNackCount(
      const QuicSentPacketManager* sent_packet_manager,
      QuicPacketSequenceNumber sequence_number);

  static size_t GetPendingRetransmissionCount(
      const QuicSentPacketManager* sent_packet_manager);

  static bool HasPendingPackets(
      const QuicSentPacketManager* sent_packet_manager);

  static QuicTime GetSentTime(const QuicSentPacketManager* sent_packet_manager,
                              QuicPacketSequenceNumber sequence_number);

  // Returns true if |sequence_number| is a retransmission of a packet.
  static bool IsRetransmission(QuicSentPacketManager* sent_packet_manager,
                               QuicPacketSequenceNumber sequence_number);

  static void MarkForRetransmission(QuicSentPacketManager* sent_packet_manager,
                                    QuicPacketSequenceNumber sequence_number,
                                    TransmissionType transmission_type);

  static QuicTime::Delta GetRetransmissionDelay(
      const QuicSentPacketManager* sent_packet_manager);

  static bool HasUnackedCryptoPackets(
      const QuicSentPacketManager* sent_packet_manager);

  static size_t GetNumRetransmittablePackets(
      const QuicSentPacketManager* sent_packet_manager);

  static QuicByteCount GetBytesInFlight(
      const QuicSentPacketManager* sent_packet_manager);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicSentPacketManagerPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_
