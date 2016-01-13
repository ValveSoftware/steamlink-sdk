// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_received_packet_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/linked_hash_map.h"
#include "net/quic/quic_connection_stats.h"

using std::make_pair;
using std::max;
using std::min;

namespace net {

namespace {

// The maximum number of packets to ack immediately after a missing packet for
// fast retransmission to kick in at the sender.  This limit is created to
// reduce the number of acks sent that have no benefit for fast retransmission.
// Set to the number of nacks needed for fast retransmit plus one for protection
// against an ack loss
const size_t kMaxPacketsAfterNewMissing = 4;

}

QuicReceivedPacketManager::EntropyTracker::EntropyTracker()
    :  packets_entropy_hash_(0),
       first_gap_(1),
       largest_observed_(0) {
}

QuicReceivedPacketManager::EntropyTracker::~EntropyTracker() {}

QuicPacketEntropyHash QuicReceivedPacketManager::EntropyTracker::EntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK_LE(sequence_number, largest_observed_);
  if (sequence_number == largest_observed_) {
    return packets_entropy_hash_;
  }

  DCHECK_GE(sequence_number, first_gap_);
  ReceivedEntropyMap::const_iterator it =
      packets_entropy_.upper_bound(sequence_number);
  // When this map is empty we should only query entropy for
  // largest_observed_, since no other entropy can be correctly
  // calculated, because we're not storing the entropy for any prior packets.
  // TODO(rtenneti): add support for LOG_IF_EVERY_N_SEC to chromium.
  // LOG_IF_EVERY_N_SEC(DFATAL, it == packets_entropy_.end(), 10)
  LOG_IF(DFATAL, it == packets_entropy_.end())
      << "EntropyHash may be unknown. largest_received: "
      << largest_observed_
      << " sequence_number: " << sequence_number;

  // TODO(satyamshekhar): Make this O(1).
  QuicPacketEntropyHash hash = packets_entropy_hash_;
  for (; it != packets_entropy_.end(); ++it) {
    hash ^= it->second;
  }
  return hash;
}

void QuicReceivedPacketManager::EntropyTracker::RecordPacketEntropyHash(
    QuicPacketSequenceNumber sequence_number,
    QuicPacketEntropyHash entropy_hash) {
  if (sequence_number < first_gap_) {
    DVLOG(1) << "Ignoring received packet entropy for sequence_number:"
             << sequence_number << " less than largest_peer_sequence_number:"
             << first_gap_;
    return;
  }

  if (sequence_number > largest_observed_) {
    largest_observed_ = sequence_number;
  }

  packets_entropy_hash_ ^= entropy_hash;
  DVLOG(2) << "setting cumulative received entropy hash to: "
           << static_cast<int>(packets_entropy_hash_)
           << " updated with sequence number " << sequence_number
           << " entropy hash: " << static_cast<int>(entropy_hash);

  packets_entropy_.insert(make_pair(sequence_number, entropy_hash));
  AdvanceFirstGapAndGarbageCollectEntropyMap();
}

void QuicReceivedPacketManager::EntropyTracker::SetCumulativeEntropyUpTo(
    QuicPacketSequenceNumber sequence_number,
    QuicPacketEntropyHash entropy_hash) {
  DCHECK_LE(sequence_number, largest_observed_);
  if (sequence_number < first_gap_) {
    DVLOG(1) << "Ignoring set entropy at:" << sequence_number
             << " less than first_gap_:" << first_gap_;
    return;
  }
  // Compute the current entropy based on the hash.
  packets_entropy_hash_ = entropy_hash;
  ReceivedEntropyMap::iterator it =
      packets_entropy_.lower_bound(sequence_number);
  // TODO(satyamshekhar): Make this O(1).
  for (; it != packets_entropy_.end(); ++it) {
    packets_entropy_hash_ ^= it->second;
  }
  // Update first_gap_ and discard old entropies.
  first_gap_ = sequence_number;
  packets_entropy_.erase(
      packets_entropy_.begin(),
      packets_entropy_.lower_bound(sequence_number));

  // Garbage collect entries from the beginning of the map.
  AdvanceFirstGapAndGarbageCollectEntropyMap();
}

void QuicReceivedPacketManager::EntropyTracker::
AdvanceFirstGapAndGarbageCollectEntropyMap() {
  while (!packets_entropy_.empty()) {
    ReceivedEntropyMap::iterator it = packets_entropy_.begin();
    if (it->first != first_gap_) {
      DCHECK_GT(it->first, first_gap_);
      break;
    }
    packets_entropy_.erase(it);
    ++first_gap_;
  }
}

QuicReceivedPacketManager::QuicReceivedPacketManager(
    CongestionFeedbackType congestion_type,
    QuicConnectionStats* stats)
    : peer_largest_observed_packet_(0),
      least_packet_awaited_by_peer_(1),
      peer_least_packet_awaiting_ack_(0),
      time_largest_observed_(QuicTime::Zero()),
      receive_algorithm_(ReceiveAlgorithmInterface::Create(congestion_type)),
      stats_(stats) {
  received_info_.largest_observed = 0;
  received_info_.entropy_hash = 0;
}

QuicReceivedPacketManager::~QuicReceivedPacketManager() {}

void QuicReceivedPacketManager::RecordPacketReceived(
    QuicByteCount bytes,
    const QuicPacketHeader& header,
    QuicTime receipt_time) {
  QuicPacketSequenceNumber sequence_number = header.packet_sequence_number;
  DCHECK(IsAwaitingPacket(sequence_number));

  InsertMissingPacketsBetween(
      &received_info_,
      max(received_info_.largest_observed + 1, peer_least_packet_awaiting_ack_),
      sequence_number);

  if (received_info_.largest_observed > sequence_number) {
    // We've gotten one of the out of order packets - remove it from our
    // "missing packets" list.
    DVLOG(1) << "Removing " << sequence_number << " from missing list";
    received_info_.missing_packets.erase(sequence_number);

    // Record how out of order stats.
    ++stats_->packets_reordered;
    uint32 sequence_gap = received_info_.largest_observed - sequence_number;
    stats_->max_sequence_reordering =
        max(stats_->max_sequence_reordering, sequence_gap);
    uint32 reordering_time_us =
        receipt_time.Subtract(time_largest_observed_).ToMicroseconds();
    stats_->max_time_reordering_us = max(stats_->max_time_reordering_us,
                                         reordering_time_us);
  }
  if (sequence_number > received_info_.largest_observed) {
    received_info_.largest_observed = sequence_number;
    time_largest_observed_ = receipt_time;
  }
  entropy_tracker_.RecordPacketEntropyHash(sequence_number,
                                           header.entropy_hash);

  receive_algorithm_->RecordIncomingPacket(
      bytes, sequence_number, receipt_time);
}

void QuicReceivedPacketManager::RecordPacketRevived(
    QuicPacketSequenceNumber sequence_number) {
  LOG_IF(DFATAL, !IsAwaitingPacket(sequence_number));
  received_info_.revived_packets.insert(sequence_number);
}

bool QuicReceivedPacketManager::IsMissing(
    QuicPacketSequenceNumber sequence_number) {
  return ContainsKey(received_info_.missing_packets, sequence_number);
}

bool QuicReceivedPacketManager::IsAwaitingPacket(
    QuicPacketSequenceNumber sequence_number) {
  return ::net::IsAwaitingPacket(received_info_, sequence_number);
}

void QuicReceivedPacketManager::UpdateReceivedPacketInfo(
    ReceivedPacketInfo* received_info,
    QuicTime approximate_now) {
  *received_info = received_info_;
  received_info->entropy_hash = EntropyHash(received_info_.largest_observed);

  if (time_largest_observed_ == QuicTime::Zero()) {
    // We have received no packets.
    received_info->delta_time_largest_observed = QuicTime::Delta::Infinite();
    return;
  }

  if (approximate_now < time_largest_observed_) {
    // Approximate now may well be "in the past".
    received_info->delta_time_largest_observed = QuicTime::Delta::Zero();
    return;
  }

  received_info->delta_time_largest_observed =
      approximate_now.Subtract(time_largest_observed_);
}

bool QuicReceivedPacketManager::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  return receive_algorithm_->GenerateCongestionFeedback(feedback);
}

QuicPacketEntropyHash QuicReceivedPacketManager::EntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  return entropy_tracker_.EntropyHash(sequence_number);
}

void QuicReceivedPacketManager::UpdatePacketInformationReceivedByPeer(
    const ReceivedPacketInfo& received_info) {
  // ValidateAck should fail if largest_observed ever shrinks.
  DCHECK_LE(peer_largest_observed_packet_, received_info.largest_observed);
  peer_largest_observed_packet_ = received_info.largest_observed;

  if (received_info.missing_packets.empty()) {
    least_packet_awaited_by_peer_ = peer_largest_observed_packet_ + 1;
  } else {
    least_packet_awaited_by_peer_ = *(received_info.missing_packets.begin());
  }
}

bool QuicReceivedPacketManager::DontWaitForPacketsBefore(
    QuicPacketSequenceNumber least_unacked) {
  received_info_.revived_packets.erase(
      received_info_.revived_packets.begin(),
      received_info_.revived_packets.lower_bound(least_unacked));
  size_t missing_packets_count = received_info_.missing_packets.size();
  received_info_.missing_packets.erase(
      received_info_.missing_packets.begin(),
      received_info_.missing_packets.lower_bound(least_unacked));
  return missing_packets_count != received_info_.missing_packets.size();
}

void QuicReceivedPacketManager::UpdatePacketInformationSentByPeer(
    const QuicStopWaitingFrame& stop_waiting) {
  // ValidateAck() should fail if peer_least_packet_awaiting_ack_ shrinks.
  DCHECK_LE(peer_least_packet_awaiting_ack_, stop_waiting.least_unacked);
  if (stop_waiting.least_unacked > peer_least_packet_awaiting_ack_) {
    bool missed_packets = DontWaitForPacketsBefore(stop_waiting.least_unacked);
    if (missed_packets) {
      DVLOG(1) << "Updating entropy hashed since we missed packets";
      // There were some missing packets that we won't ever get now. Recalculate
      // the received entropy hash.
      entropy_tracker_.SetCumulativeEntropyUpTo(stop_waiting.least_unacked,
                                                stop_waiting.entropy_hash);
    }
    peer_least_packet_awaiting_ack_ = stop_waiting.least_unacked;
  }
  DCHECK(received_info_.missing_packets.empty() ||
         *received_info_.missing_packets.begin() >=
             peer_least_packet_awaiting_ack_);
}

bool QuicReceivedPacketManager::HasMissingPackets() {
  return !received_info_.missing_packets.empty();
}

bool QuicReceivedPacketManager::HasNewMissingPackets() {
  return HasMissingPackets() &&
      (received_info_.largest_observed -
       *received_info_.missing_packets.rbegin()) <= kMaxPacketsAfterNewMissing;
}

}  // namespace net
