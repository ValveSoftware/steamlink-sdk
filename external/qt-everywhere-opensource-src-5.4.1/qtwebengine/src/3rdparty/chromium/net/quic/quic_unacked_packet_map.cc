// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_unacked_packet_map.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_utils_chromium.h"

using std::max;

namespace net {

QuicUnackedPacketMap::QuicUnackedPacketMap()
    : largest_sent_packet_(0),
      largest_observed_(0),
      bytes_in_flight_(0),
      pending_crypto_packet_count_(0) {
}

QuicUnackedPacketMap::~QuicUnackedPacketMap() {
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    delete it->second.retransmittable_frames;
    // Only delete all_transmissions once, for the newest packet.
    if (it->first == *it->second.all_transmissions->rbegin()) {
      delete it->second.all_transmissions;
    }
  }
}

// TODO(ianswett): Combine this method with OnPacketSent once packets are always
// sent in order and the connection tracks RetransmittableFrames for longer.
void QuicUnackedPacketMap::AddPacket(
    const SerializedPacket& serialized_packet) {
  if (!unacked_packets_.empty()) {
    bool is_old_packet = unacked_packets_.rbegin()->first >=
        serialized_packet.sequence_number;
    LOG_IF(DFATAL, is_old_packet) << "Old packet serialized: "
                                  << serialized_packet.sequence_number
                                  << " vs: "
                                  << unacked_packets_.rbegin()->first;
  }

  unacked_packets_[serialized_packet.sequence_number] =
      TransmissionInfo(serialized_packet.retransmittable_frames,
                       serialized_packet.sequence_number,
                       serialized_packet.sequence_number_length);
  if (serialized_packet.retransmittable_frames != NULL &&
      serialized_packet.retransmittable_frames->HasCryptoHandshake()
          == IS_HANDSHAKE) {
    ++pending_crypto_packet_count_;
  }
}

void QuicUnackedPacketMap::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number,
    TransmissionType transmission_type) {
  DCHECK(ContainsKey(unacked_packets_, old_sequence_number));
  DCHECK(unacked_packets_.empty() ||
         unacked_packets_.rbegin()->first < new_sequence_number);

  // TODO(ianswett): Discard and lose the packet lazily instead of immediately.
  TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, old_sequence_number);
  RetransmittableFrames* frames = transmission_info->retransmittable_frames;
  LOG_IF(DFATAL, frames == NULL) << "Attempt to retransmit packet with no "
                                 << "retransmittable frames: "
                                 << old_sequence_number;

  // We keep the old packet in the unacked packet list until it, or one of
  // the retransmissions of it are acked.
  transmission_info->retransmittable_frames = NULL;
  unacked_packets_[new_sequence_number] =
      TransmissionInfo(frames,
                       new_sequence_number,
                       transmission_info->sequence_number_length,
                       transmission_type,
                       transmission_info->all_transmissions);
}

void QuicUnackedPacketMap::ClearPreviousRetransmissions(size_t num_to_clear) {
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end() && num_to_clear > 0) {
    QuicPacketSequenceNumber sequence_number = it->first;
    // If this packet is in flight, or has retransmittable data, then there is
    // no point in clearing out any further packets, because they would not
    // affect the high water mark.
    if (it->second.in_flight || it->second.retransmittable_frames != NULL) {
      break;
    }

    it->second.all_transmissions->erase(sequence_number);
    LOG_IF(DFATAL, it->second.all_transmissions->empty())
        << "Previous retransmissions must have a newer transmission.";
    ++it;
    unacked_packets_.erase(sequence_number);
    --num_to_clear;
  }
}

bool QuicUnackedPacketMap::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  const TransmissionInfo* transmission_info =
      FindOrNull(unacked_packets_, sequence_number);
  if (transmission_info == NULL) {
    return false;
  }

  return transmission_info->retransmittable_frames != NULL;
}

void QuicUnackedPacketMap::NackPacket(QuicPacketSequenceNumber sequence_number,
                                      size_t min_nacks) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "NackPacket called for packet that is not unacked: "
                << sequence_number;
    return;
  }

  it->second.nack_count = max(min_nacks, it->second.nack_count);
}

void QuicUnackedPacketMap::RemoveRetransmittability(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    DVLOG(1) << "packet is not in unacked_packets: " << sequence_number;
    return;
  }
  SequenceNumberSet* all_transmissions = it->second.all_transmissions;
  // TODO(ianswett): Consider optimizing this for lone packets.
  // TODO(ianswett): Consider adding a check to ensure there are retransmittable
  // frames associated with this packet.
  for (SequenceNumberSet::reverse_iterator it = all_transmissions->rbegin();
       it != all_transmissions->rend(); ++it) {
    TransmissionInfo* transmission_info = FindOrNull(unacked_packets_, *it);
    if (transmission_info == NULL) {
      LOG(DFATAL) << "All transmissions in all_transmissions must be present "
                  << "in the unacked packet map.";
      continue;
    }
    MaybeRemoveRetransmittableFrames(transmission_info);
    if (*it <= largest_observed_ && !transmission_info->in_flight) {
      unacked_packets_.erase(*it);
    } else {
      transmission_info->all_transmissions = new SequenceNumberSet();
      transmission_info->all_transmissions->insert(*it);
    }
  }

  delete all_transmissions;
}

void QuicUnackedPacketMap::MaybeRemoveRetransmittableFrames(
    TransmissionInfo* transmission_info) {
  if (transmission_info->retransmittable_frames != NULL) {
    if (transmission_info->retransmittable_frames->HasCryptoHandshake()
            == IS_HANDSHAKE) {
      --pending_crypto_packet_count_;
    }
    delete transmission_info->retransmittable_frames;
    transmission_info->retransmittable_frames = NULL;
  }
}

void QuicUnackedPacketMap::IncreaseLargestObserved(
    QuicPacketSequenceNumber largest_observed) {
  DCHECK_LT(largest_observed_, largest_observed);
  largest_observed_ = largest_observed;
  UnackedPacketMap::iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end() && it->first <= largest_observed_) {
    if (!IsPacketUseless(it)) {
      ++it;
      continue;
    }
    delete it->second.all_transmissions;
    QuicPacketSequenceNumber sequence_number = it->first;
    ++it;
    unacked_packets_.erase(sequence_number);
  }
}

bool QuicUnackedPacketMap::IsPacketUseless(
    UnackedPacketMap::const_iterator it) const {
  return it->first <= largest_observed_ &&
      !it->second.in_flight &&
      it->second.retransmittable_frames == NULL &&
      it->second.all_transmissions->size() == 1;
}

bool QuicUnackedPacketMap::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(unacked_packets_, sequence_number);
}

void QuicUnackedPacketMap::RemoveFromInFlight(
    QuicPacketSequenceNumber sequence_number) {
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "RemoveFromFlight called for packet that is not unacked: "
                << sequence_number;
    return;
  }
  if (it->second.in_flight) {
    LOG_IF(DFATAL, bytes_in_flight_ < it->second.bytes_sent);
    bytes_in_flight_ -= it->second.bytes_sent;
    it->second.in_flight = false;
  }
  if (IsPacketUseless(it)) {
    delete it->second.all_transmissions;
    unacked_packets_.erase(it);
  }
}

bool QuicUnackedPacketMap::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

bool QuicUnackedPacketMap::HasInFlightPackets() const {
  return bytes_in_flight_ > 0;
}

const TransmissionInfo& QuicUnackedPacketMap::GetTransmissionInfo(
    QuicPacketSequenceNumber sequence_number) const {
  return unacked_packets_.find(sequence_number)->second;
}

QuicTime QuicUnackedPacketMap::GetLastPacketSentTime() const {
  UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
  while (it != unacked_packets_.rend()) {
    if (it->second.in_flight) {
      LOG_IF(DFATAL, it->second.sent_time == QuicTime::Zero())
          << "Sent time can never be zero for a packet in flight.";
      return it->second.sent_time;
    }
    ++it;
  }
  LOG(DFATAL) << "GetLastPacketSentTime requires in flight packets.";
  return QuicTime::Zero();
}

QuicTime QuicUnackedPacketMap::GetFirstInFlightPacketSentTime() const {
  UnackedPacketMap::const_iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end() && !it->second.in_flight) {
    ++it;
  }
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "GetFirstInFlightPacketSentTime requires in flight packets.";
    return QuicTime::Zero();
  }
  return it->second.sent_time;
}

size_t QuicUnackedPacketMap::GetNumUnackedPackets() const {
  return unacked_packets_.size();
}

bool QuicUnackedPacketMap::HasMultipleInFlightPackets() const {
  size_t num_in_flight = 0;
  for (UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
       it != unacked_packets_.rend(); ++it) {
    if (it->second.in_flight) {
      ++num_in_flight;
    }
    if (num_in_flight > 1) {
      return true;
    }
  }
  return false;
}

bool QuicUnackedPacketMap::HasPendingCryptoPackets() const {
  return pending_crypto_packet_count_ > 0;
}

bool QuicUnackedPacketMap::HasUnackedRetransmittableFrames() const {
  for (UnackedPacketMap::const_reverse_iterator it =
           unacked_packets_.rbegin(); it != unacked_packets_.rend(); ++it) {
    if (it->second.in_flight && it->second.retransmittable_frames) {
      return true;
    }
  }
  return false;
}

QuicPacketSequenceNumber
QuicUnackedPacketMap::GetLeastUnackedSentPacket() const {
  if (unacked_packets_.empty()) {
    // If there are no unacked packets, return 0.
    return 0;
  }

  return unacked_packets_.begin()->first;
}

void QuicUnackedPacketMap::SetSent(QuicPacketSequenceNumber sequence_number,
                                   QuicTime sent_time,
                                   QuicByteCount bytes_sent,
                                   bool set_in_flight) {
  DCHECK_LT(0u, sequence_number);
  UnackedPacketMap::iterator it = unacked_packets_.find(sequence_number);
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "OnPacketSent called for packet that is not unacked: "
                << sequence_number;
    return;
  }
  DCHECK(!it->second.in_flight);

  largest_sent_packet_ = max(sequence_number, largest_sent_packet_);
  it->second.sent_time = sent_time;
  if (set_in_flight) {
    bytes_in_flight_ += bytes_sent;
    it->second.bytes_sent = bytes_sent;
    it->second.in_flight = true;
  }
}

}  // namespace net
