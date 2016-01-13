// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"

#include "base/stl_util.h"
#include "net/quic/congestion_control/loss_detection_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"

namespace net {
namespace test {

// static
void QuicSentPacketManagerPeer::SetMaxTailLossProbes(
    QuicSentPacketManager* sent_packet_manager, size_t max_tail_loss_probes) {
  sent_packet_manager->max_tail_loss_probes_ = max_tail_loss_probes;
}

// static
void QuicSentPacketManagerPeer::SetSendAlgorithm(
    QuicSentPacketManager* sent_packet_manager,
    SendAlgorithmInterface* send_algorithm) {
  sent_packet_manager->send_algorithm_.reset(send_algorithm);
}

// static
const LossDetectionInterface* QuicSentPacketManagerPeer::GetLossAlgorithm(
    QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->loss_algorithm_.get();
}

// static
void QuicSentPacketManagerPeer::SetLossAlgorithm(
    QuicSentPacketManager* sent_packet_manager,
    LossDetectionInterface* loss_detector) {
  sent_packet_manager->loss_algorithm_.reset(loss_detector);
}

// static
RttStats* QuicSentPacketManagerPeer::GetRttStats(
    QuicSentPacketManager* sent_packet_manager) {
  return &sent_packet_manager->rtt_stats_;
}

// static
size_t QuicSentPacketManagerPeer::GetNackCount(
    const QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number) {
  return sent_packet_manager->unacked_packets_.
      GetTransmissionInfo(sequence_number).nack_count;
}

// static
size_t QuicSentPacketManagerPeer::GetPendingRetransmissionCount(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->pending_retransmissions_.size();
}

// static
bool QuicSentPacketManagerPeer::HasPendingPackets(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->unacked_packets_.HasInFlightPackets();
}

// static
QuicTime QuicSentPacketManagerPeer::GetSentTime(
    const QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(sent_packet_manager->unacked_packets_.IsUnacked(sequence_number));

  return sent_packet_manager->unacked_packets_.GetTransmissionInfo(
      sequence_number).sent_time;
}

// static
bool QuicSentPacketManagerPeer::IsRetransmission(
    QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(sent_packet_manager->HasRetransmittableFrames(sequence_number));
  return sent_packet_manager->HasRetransmittableFrames(sequence_number) &&
      sent_packet_manager->unacked_packets_.GetTransmissionInfo(
          sequence_number).all_transmissions->size() > 1;
}

// static
void QuicSentPacketManagerPeer::MarkForRetransmission(
    QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  sent_packet_manager->MarkForRetransmission(sequence_number,
                                             transmission_type);
}

// static
QuicTime::Delta QuicSentPacketManagerPeer::GetRetransmissionDelay(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->GetRetransmissionDelay();
}

// static
bool QuicSentPacketManagerPeer::HasUnackedCryptoPackets(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->unacked_packets_.HasPendingCryptoPackets();
}

// static
size_t QuicSentPacketManagerPeer::GetNumRetransmittablePackets(
    const QuicSentPacketManager* sent_packet_manager) {
  size_t num_unacked_packets = 0;
  for (QuicUnackedPacketMap::const_iterator it =
           sent_packet_manager->unacked_packets_.begin();
       it != sent_packet_manager->unacked_packets_.end(); ++it) {
    if (it->second.retransmittable_frames != NULL) {
      ++num_unacked_packets;
    }
  }
  return num_unacked_packets;
}

// static
QuicByteCount QuicSentPacketManagerPeer::GetBytesInFlight(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->unacked_packets_.bytes_in_flight();
}

}  // namespace test
}  // namespace net
