// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/tcp_loss_algorithm.h"

#include "net/quic/congestion_control/rtt_stats.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace {

// TCP retransmits after 3 nacks.
static const size_t kNumberOfNacksBeforeRetransmission = 3;

// How many RTTs the algorithm waits before determining a packet is lost due
// to early retransmission.
static const double kEarlyRetransmitLossDelayMultiplier = 1.25;

}

TCPLossAlgorithm::TCPLossAlgorithm()
    : loss_detection_timeout_(QuicTime::Zero()) { }

LossDetectionType TCPLossAlgorithm::GetLossDetectionType() const {
  return kNack;
}

// Uses nack counts to decide when packets are lost.
SequenceNumberSet TCPLossAlgorithm::DetectLostPackets(
    const QuicUnackedPacketMap& unacked_packets,
    const QuicTime& time,
    QuicPacketSequenceNumber largest_observed,
    const RttStats& rtt_stats) {
  SequenceNumberSet lost_packets;
  loss_detection_timeout_ = QuicTime::Zero();
  QuicTime::Delta loss_delay =
      rtt_stats.SmoothedRtt().Multiply(kEarlyRetransmitLossDelayMultiplier);

  for (QuicUnackedPacketMap::const_iterator it = unacked_packets.begin();
       it != unacked_packets.end() && it->first <= largest_observed; ++it) {
    if (!it->second.in_flight) {
      continue;
    }

    LOG_IF(DFATAL, it->second.nack_count == 0)
        << "All packets less than largest observed should have been nacked.";
    if (it->second.nack_count >= kNumberOfNacksBeforeRetransmission) {
      lost_packets.insert(it->first);
      continue;
    }

    // Only early retransmit(RFC5827) when the last packet gets acked and
    // there are retransmittable packets in flight.
    // This also implements a timer-protected variant of FACK.
    if (it->second.retransmittable_frames &&
        unacked_packets.largest_sent_packet() == largest_observed) {
      // Early retransmit marks the packet as lost once 1.25RTTs have passed
      // since the packet was sent and otherwise sets an alarm.
      if (time >= it->second.sent_time.Add(loss_delay)) {
        lost_packets.insert(it->first);
      } else {
        // Set the timeout for the earliest retransmittable packet where early
        // retransmit applies.
        loss_detection_timeout_ = it->second.sent_time.Add(loss_delay);
        break;
      }
    }
  }

  return lost_packets;
}

QuicTime TCPLossAlgorithm::GetLossTimeout() const {
  return loss_detection_timeout_;
}

}  // namespace net
