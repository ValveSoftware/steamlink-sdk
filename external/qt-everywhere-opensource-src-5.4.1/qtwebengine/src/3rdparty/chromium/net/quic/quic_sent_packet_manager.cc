// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/pacing_sender.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_ack_notifier_manager.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils_chromium.h"

using std::make_pair;
using std::max;
using std::min;

namespace net {
namespace {
static const int kDefaultRetransmissionTimeMs = 500;
// TCP RFC calls for 1 second RTO however Linux differs from this default and
// define the minimum RTO to 200ms, we will use the same until we have data to
// support a higher or lower value.
static const int kMinRetransmissionTimeMs = 200;
static const int kMaxRetransmissionTimeMs = 60000;
static const size_t kMaxRetransmissions = 10;

// Only exponentially back off the handshake timer 5 times due to a timeout.
static const size_t kMaxHandshakeRetransmissionBackoffs = 5;
static const size_t kMinHandshakeTimeoutMs = 10;

// Sends up to two tail loss probes before firing an RTO,
// per draft RFC draft-dukkipati-tcpm-tcp-loss-probe.
static const size_t kDefaultMaxTailLossProbes = 2;
static const int64 kMinTailLossProbeTimeoutMs = 10;

// Number of samples before we force a new recent min rtt to be captured.
static const size_t kNumMinRttSamplesAfterQuiescence = 2;

bool HasCryptoHandshake(const TransmissionInfo& transmission_info) {
  if (transmission_info.retransmittable_frames == NULL) {
    return false;
  }
  return transmission_info.retransmittable_frames->HasCryptoHandshake() ==
      IS_HANDSHAKE;
}

}  // namespace

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicSentPacketManager::QuicSentPacketManager(bool is_server,
                                             const QuicClock* clock,
                                             QuicConnectionStats* stats,
                                             CongestionFeedbackType type,
                                             LossDetectionType loss_type)
    : unacked_packets_(),
      is_server_(is_server),
      clock_(clock),
      stats_(stats),
      debug_delegate_(NULL),
      send_algorithm_(
          SendAlgorithmInterface::Create(clock, &rtt_stats_, type, stats)),
      loss_algorithm_(LossDetectionInterface::Create(loss_type)),
      largest_observed_(0),
      consecutive_rto_count_(0),
      consecutive_tlp_count_(0),
      consecutive_crypto_retransmission_count_(0),
      pending_tlp_transmission_(false),
      max_tail_loss_probes_(kDefaultMaxTailLossProbes),
      using_pacing_(false) {
}

QuicSentPacketManager::~QuicSentPacketManager() {
}

void QuicSentPacketManager::SetFromConfig(const QuicConfig& config) {
  if (config.HasReceivedInitialRoundTripTimeUs() &&
      config.ReceivedInitialRoundTripTimeUs() > 0) {
    rtt_stats_.set_initial_rtt_us(min(kMaxInitialRoundTripTimeUs,
                                      config.ReceivedInitialRoundTripTimeUs()));
  }
  // TODO(ianswett): BBR is currently a server only feature.
  if (config.HasReceivedCongestionOptions() &&
      ContainsQuicTag(config.ReceivedCongestionOptions(), kTBBR)) {
    send_algorithm_.reset(
        SendAlgorithmInterface::Create(clock_, &rtt_stats_, kTCPBBR, stats_));
  }
  if (config.congestion_feedback() == kPACE) {
    MaybeEnablePacing();
  }
  if (config.HasReceivedLossDetection() &&
      config.ReceivedLossDetection() == kTIME) {
    loss_algorithm_.reset(LossDetectionInterface::Create(kTime));
  }
  send_algorithm_->SetFromConfig(config, is_server_);
}

// TODO(ianswett): Combine this method with OnPacketSent once packets are always
// sent in order and the connection tracks RetransmittableFrames for longer.
void QuicSentPacketManager::OnSerializedPacket(
    const SerializedPacket& serialized_packet) {
  if (serialized_packet.retransmittable_frames) {
    ack_notifier_manager_.OnSerializedPacket(serialized_packet);
  }

  unacked_packets_.AddPacket(serialized_packet);
}

void QuicSentPacketManager::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number) {
  TransmissionType transmission_type;
  PendingRetransmissionMap::iterator it =
      pending_retransmissions_.find(old_sequence_number);
  if (it != pending_retransmissions_.end()) {
    transmission_type = it->second;
    pending_retransmissions_.erase(it);
  } else {
    DLOG(DFATAL) << "Expected sequence number to be in "
        "pending_retransmissions_.  sequence_number: " << old_sequence_number;
    transmission_type = NOT_RETRANSMISSION;
  }

  // A notifier may be waiting to hear about ACKs for the original sequence
  // number. Inform them that the sequence number has changed.
  ack_notifier_manager_.UpdateSequenceNumber(old_sequence_number,
                                             new_sequence_number);

  unacked_packets_.OnRetransmittedPacket(old_sequence_number,
                                         new_sequence_number,
                                         transmission_type);
}

void QuicSentPacketManager::OnIncomingAck(
    const ReceivedPacketInfo& received_info,
    QuicTime ack_receive_time) {
  QuicByteCount bytes_in_flight = unacked_packets_.bytes_in_flight();

  // We rely on delta_time_largest_observed to compute an RTT estimate, so
  // we only update rtt when the largest observed gets acked.
  bool largest_observed_acked = MaybeUpdateRTT(received_info, ack_receive_time);
  if (largest_observed_ < received_info.largest_observed) {
    largest_observed_ = received_info.largest_observed;
    unacked_packets_.IncreaseLargestObserved(largest_observed_);
  }
  HandleAckForSentPackets(received_info);
  InvokeLossDetection(ack_receive_time);
  MaybeInvokeCongestionEvent(largest_observed_acked, bytes_in_flight);

  // If we have received a truncated ack, then we need to clear out some
  // previous transmissions to allow the peer to actually ACK new packets.
  if (received_info.is_truncated) {
    unacked_packets_.ClearPreviousRetransmissions(
        received_info.missing_packets.size() / 2);
  }

  // Anytime we are making forward progress and have a new RTT estimate, reset
  // the backoff counters.
  if (largest_observed_acked) {
    // Reset all retransmit counters any time a new packet is acked.
    consecutive_rto_count_ = 0;
    consecutive_tlp_count_ = 0;
    consecutive_crypto_retransmission_count_ = 0;
  }
}

void QuicSentPacketManager::MaybeInvokeCongestionEvent(
    bool rtt_updated, QuicByteCount bytes_in_flight) {
  if (rtt_updated || !packets_acked_.empty() ||
      !packets_lost_.empty()) {
    send_algorithm_->OnCongestionEvent(
        rtt_updated, bytes_in_flight, packets_acked_, packets_lost_);
    packets_acked_.clear();
    packets_lost_.clear();
  }
}

void QuicSentPacketManager::HandleAckForSentPackets(
    const ReceivedPacketInfo& received_info) {
  // Go through the packets we have not received an ack for and see if this
  // incoming_ack shows they've been seen by the peer.
  QuicTime::Delta delta_largest_observed =
      received_info.delta_time_largest_observed;
  QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (sequence_number > received_info.largest_observed) {
      // These packets are still in flight.
      break;
    }

    if (IsAwaitingPacket(received_info, sequence_number)) {
      // Consider it multiple nacks when there is a gap between the missing
      // packet and the largest observed, since the purpose of a nack
      // threshold is to tolerate re-ordering.  This handles both StretchAcks
      // and Forward Acks.
      // The nack count only increases when the largest observed increases.
      size_t min_nacks = received_info.largest_observed - sequence_number;
      // Truncated acks can nack the largest observed, so use a min of 1.
      if (min_nacks == 0) {
        min_nacks = 1;
      }
      unacked_packets_.NackPacket(sequence_number, min_nacks);
      ++it;
      continue;
    }
    // Packet was acked, so remove it from our unacked packet list.
    DVLOG(1) << ENDPOINT << "Got an ack for packet " << sequence_number;
    // If data is associated with the most recent transmission of this
    // packet, then inform the caller.
    if (it->second.in_flight) {
      packets_acked_[sequence_number] = it->second;
    }
    it = MarkPacketHandled(it, delta_largest_observed);
  }

  // Discard any retransmittable frames associated with revived packets.
  for (SequenceNumberSet::const_iterator revived_it =
           received_info.revived_packets.begin();
       revived_it != received_info.revived_packets.end(); ++revived_it) {
    MarkPacketRevived(*revived_it, delta_largest_observed);
  }
}

bool QuicSentPacketManager::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  return unacked_packets_.HasRetransmittableFrames(sequence_number);
}

void QuicSentPacketManager::RetransmitUnackedPackets(
    RetransmissionType retransmission_type) {
  QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    const RetransmittableFrames* frames = it->second.retransmittable_frames;
    // TODO(ianswett): Consider adding a new retransmission type which removes
    // all these old packets from unacked and retransmits them as new sequence
    // numbers with no connection to the previous ones.
    if (frames != NULL && (retransmission_type == ALL_PACKETS ||
                           frames->encryption_level() == ENCRYPTION_INITIAL)) {
      MarkForRetransmission(it->first, ALL_UNACKED_RETRANSMISSION);
    }
    ++it;
  }
}

void QuicSentPacketManager::NeuterUnencryptedPackets() {
  QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    const RetransmittableFrames* frames = it->second.retransmittable_frames;
    QuicPacketSequenceNumber sequence_number = it->first;
    ++it;
    if (frames != NULL && frames->encryption_level() == ENCRYPTION_NONE) {
      // Once you're forward secure, no unencrypted packets will be sent, crypto
      // or otherwise. Unencrypted packets are neutered and abandoned, to ensure
      // they are not retransmitted or considered lost from a congestion control
      // perspective.
      pending_retransmissions_.erase(sequence_number);
      unacked_packets_.RemoveFromInFlight(sequence_number);
      // RemoveRetransmittibility is safe because only the newest sequence
      // number can have frames.
      unacked_packets_.RemoveRetransmittability(sequence_number);
    }
  }
}

void QuicSentPacketManager::MarkForRetransmission(
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(sequence_number);
  LOG_IF(DFATAL, transmission_info.retransmittable_frames == NULL);
  if (transmission_type != TLP_RETRANSMISSION) {
    unacked_packets_.RemoveFromInFlight(sequence_number);
  }
  // TODO(ianswett): Currently the RTO can fire while there are pending NACK
  // retransmissions for the same data, which is not ideal.
  if (ContainsKey(pending_retransmissions_, sequence_number)) {
    return;
  }

  pending_retransmissions_[sequence_number] = transmission_type;
}

void QuicSentPacketManager::RecordSpuriousRetransmissions(
    const SequenceNumberSet& all_transmissions,
    QuicPacketSequenceNumber acked_sequence_number) {
  for (SequenceNumberSet::const_iterator
           it = all_transmissions.upper_bound(acked_sequence_number),
           end = all_transmissions.end();
       it != end;
       ++it) {
    const TransmissionInfo& retransmit_info =
        unacked_packets_.GetTransmissionInfo(*it);

    stats_->bytes_spuriously_retransmitted += retransmit_info.bytes_sent;
    ++stats_->packets_spuriously_retransmitted;
    if (debug_delegate_ != NULL) {
      debug_delegate_->OnSpuriousPacketRetransmition(
          retransmit_info.transmission_type,
          retransmit_info.bytes_sent);
    }
  }
}

bool QuicSentPacketManager::HasPendingRetransmissions() const {
  return !pending_retransmissions_.empty();
}

QuicSentPacketManager::PendingRetransmission
    QuicSentPacketManager::NextPendingRetransmission() {
  DCHECK(!pending_retransmissions_.empty());
  QuicPacketSequenceNumber sequence_number =
      pending_retransmissions_.begin()->first;
  TransmissionType transmission_type = pending_retransmissions_.begin()->second;
  if (unacked_packets_.HasPendingCryptoPackets()) {
    // Ensure crypto packets are retransmitted before other packets.
    PendingRetransmissionMap::const_iterator it =
        pending_retransmissions_.begin();
    do {
      if (HasCryptoHandshake(unacked_packets_.GetTransmissionInfo(it->first))) {
        sequence_number = it->first;
        transmission_type = it->second;
        break;
      }
      ++it;
    } while (it != pending_retransmissions_.end());
  }
  DCHECK(unacked_packets_.IsUnacked(sequence_number)) << sequence_number;
  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(sequence_number);
  DCHECK(transmission_info.retransmittable_frames);

  return PendingRetransmission(sequence_number,
                               transmission_type,
                               *transmission_info.retransmittable_frames,
                               transmission_info.sequence_number_length);
}

void QuicSentPacketManager::MarkPacketRevived(
    QuicPacketSequenceNumber sequence_number,
    QuicTime::Delta delta_largest_observed) {
  if (!unacked_packets_.IsUnacked(sequence_number)) {
    return;
  }

  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(sequence_number);
  QuicPacketSequenceNumber newest_transmission =
      *transmission_info.all_transmissions->rbegin();
  // This packet has been revived at the receiver. If we were going to
  // retransmit it, do not retransmit it anymore.
  pending_retransmissions_.erase(newest_transmission);

  // The AckNotifierManager needs to be notified for revived packets,
  // since it indicates the packet arrived from the appliction's perspective.
  if (transmission_info.retransmittable_frames) {
    ack_notifier_manager_.OnPacketAcked(
        newest_transmission, delta_largest_observed);
  }

  unacked_packets_.RemoveRetransmittability(sequence_number);
}

QuicUnackedPacketMap::const_iterator QuicSentPacketManager::MarkPacketHandled(
    QuicUnackedPacketMap::const_iterator it,
    QuicTime::Delta delta_largest_observed) {
  LOG_IF(DFATAL, it == unacked_packets_.end())
      << "MarkPacketHandled must be passed a valid iterator entry.";
  const QuicPacketSequenceNumber sequence_number = it->first;
  const TransmissionInfo& transmission_info = it->second;

  QuicPacketSequenceNumber newest_transmission =
      *transmission_info.all_transmissions->rbegin();
  // Remove the most recent packet, if it is pending retransmission.
  pending_retransmissions_.erase(newest_transmission);

  // Notify observers about the ACKed packet.
  {
    // The AckNotifierManager needs to be notified about the most recent
    // transmission, since that's the one only one it tracks.
    ack_notifier_manager_.OnPacketAcked(newest_transmission,
                                        delta_largest_observed);
    if (newest_transmission != sequence_number) {
      RecordSpuriousRetransmissions(*transmission_info.all_transmissions,
                                    sequence_number);
    }
  }

  // Two cases for MarkPacketHandled:
  // 1) Handle the most recent or a crypto packet, so remove all transmissions.
  // 2) Handle old transmission, keep all other pending transmissions,
  //    but disassociate them from one another.

  // If it's a crypto handshake packet, discard it and all retransmissions,
  // since they won't be acked now that one has been processed.
  // TODO(ianswett): Instead of handling all crypto packets in a special way,
  // only handle NULL encrypted packets in a special way.
  if (HasCryptoHandshake(
          unacked_packets_.GetTransmissionInfo(newest_transmission))) {
    unacked_packets_.RemoveFromInFlight(newest_transmission);
  }
  unacked_packets_.RemoveFromInFlight(sequence_number);
  unacked_packets_.RemoveRetransmittability(sequence_number);

  QuicUnackedPacketMap::const_iterator next_unacked = unacked_packets_.begin();
  while (next_unacked != unacked_packets_.end() &&
         next_unacked->first <= sequence_number) {
    ++next_unacked;
  }
  return next_unacked;
}

bool QuicSentPacketManager::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  return unacked_packets_.IsUnacked(sequence_number);
}

bool QuicSentPacketManager::HasUnackedPackets() const {
  return unacked_packets_.HasUnackedPackets();
}

QuicPacketSequenceNumber
QuicSentPacketManager::GetLeastUnackedSentPacket() const {
  return unacked_packets_.GetLeastUnackedSentPacket();
}

bool QuicSentPacketManager::OnPacketSent(
    QuicPacketSequenceNumber sequence_number,
    QuicTime sent_time,
    QuicByteCount bytes,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  DCHECK_LT(0u, sequence_number);
  LOG_IF(DFATAL, bytes == 0) << "Cannot send empty packets.";
  pending_tlp_transmission_ = false;
  // In rare circumstances, the packet could be serialized, sent, and then acked
  // before OnPacketSent is called.
  if (!unacked_packets_.IsUnacked(sequence_number)) {
    return false;
  }

  if (unacked_packets_.bytes_in_flight() == 0) {
    // TODO(ianswett): Consider being less aggressive to force a new
    // recent_min_rtt, likely by not discarding a relatively new sample.
    DVLOG(1) << "Sampling a new recent min rtt within 2 samples. currently:"
             << rtt_stats_.recent_min_rtt().ToMilliseconds() << "ms";
    rtt_stats_.SampleNewRecentMinRtt(kNumMinRttSamplesAfterQuiescence);
  }

  // Only track packets as in flight that the send algorithm wants us to track.
  const bool in_flight =
      send_algorithm_->OnPacketSent(sent_time,
                                    unacked_packets_.bytes_in_flight(),
                                    sequence_number,
                                    bytes,
                                    has_retransmittable_data);
  unacked_packets_.SetSent(sequence_number, sent_time, bytes, in_flight);

  // Reset the retransmission timer anytime a pending packet is sent.
  return in_flight;
}

void QuicSentPacketManager::OnRetransmissionTimeout() {
  DCHECK(unacked_packets_.HasInFlightPackets());
  DCHECK(!pending_tlp_transmission_);
  // Handshake retransmission, timer based loss detection, TLP, and RTO are
  // implemented with a single alarm. The handshake alarm is set when the
  // handshake has not completed, the loss alarm is set when the loss detection
  // algorithm says to, and the TLP and  RTO alarms are set after that.
  // The TLP alarm is always set to run for under an RTO.
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      ++stats_->crypto_retransmit_count;
      RetransmitCryptoPackets();
      return;
    case LOSS_MODE: {
      ++stats_->loss_timeout_count;
      QuicByteCount bytes_in_flight = unacked_packets_.bytes_in_flight();
      InvokeLossDetection(clock_->Now());
      MaybeInvokeCongestionEvent(false, bytes_in_flight);
      return;
    }
    case TLP_MODE:
      // If no tail loss probe can be sent, because there are no retransmittable
      // packets, execute a conventional RTO to abandon old packets.
      ++stats_->tlp_count;
      ++consecutive_tlp_count_;
      pending_tlp_transmission_ = true;
      // TLPs prefer sending new data instead of retransmitting data, so
      // give the connection a chance to write before completing the TLP.
      return;
    case RTO_MODE:
      ++stats_->rto_count;
      RetransmitAllPackets();
      return;
  }
}

void QuicSentPacketManager::RetransmitCryptoPackets() {
  DCHECK_EQ(HANDSHAKE_MODE, GetRetransmissionMode());
  // TODO(ianswett): Typical TCP implementations only retransmit 5 times.
  consecutive_crypto_retransmission_count_ =
      min(kMaxHandshakeRetransmissionBackoffs,
          consecutive_crypto_retransmission_count_ + 1);
  bool packet_retransmitted = false;
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = it->first;
    const RetransmittableFrames* frames = it->second.retransmittable_frames;
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->second.in_flight || frames == NULL ||
        frames->HasCryptoHandshake() != IS_HANDSHAKE) {
      continue;
    }
    packet_retransmitted = true;
    MarkForRetransmission(sequence_number, HANDSHAKE_RETRANSMISSION);
  }
  DCHECK(packet_retransmitted) << "No crypto packets found to retransmit.";
}

bool QuicSentPacketManager::MaybeRetransmitTailLossProbe() {
  if (!pending_tlp_transmission_) {
    return false;
  }
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = it->first;
    const RetransmittableFrames* frames = it->second.retransmittable_frames;
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->second.in_flight || frames == NULL) {
      continue;
    }
    DCHECK_NE(IS_HANDSHAKE, frames->HasCryptoHandshake());
    MarkForRetransmission(sequence_number, TLP_RETRANSMISSION);
    return true;
  }
  DLOG(FATAL)
    << "No retransmittable packets, so RetransmitOldestPacket failed.";
  return false;
}

void QuicSentPacketManager::RetransmitAllPackets() {
  DVLOG(1) << "RetransmitAllPackets() called with "
           << unacked_packets_.GetNumUnackedPackets() << " unacked packets.";
  // Request retransmission of all retransmittable packets when the RTO
  // fires, and let the congestion manager decide how many to send
  // immediately and the remaining packets will be queued.
  // Abandon any non-retransmittable packets that are sufficiently old.
  bool packets_retransmitted = false;
  QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end()) {
    const RetransmittableFrames* frames = it->second.retransmittable_frames;
    QuicPacketSequenceNumber sequence_number = it->first;
    ++it;
    if (frames != NULL) {
      packets_retransmitted = true;
      MarkForRetransmission(sequence_number, RTO_RETRANSMISSION);
    } else {
      unacked_packets_.RemoveFromInFlight(sequence_number);
    }
  }

  send_algorithm_->OnRetransmissionTimeout(packets_retransmitted);
  if (packets_retransmitted) {
    ++consecutive_rto_count_;
  }
}

QuicSentPacketManager::RetransmissionTimeoutMode
    QuicSentPacketManager::GetRetransmissionMode() const {
  DCHECK(unacked_packets_.HasInFlightPackets());
  if (unacked_packets_.HasPendingCryptoPackets()) {
    return HANDSHAKE_MODE;
  }
  if (loss_algorithm_->GetLossTimeout() != QuicTime::Zero()) {
    return LOSS_MODE;
  }
  if (consecutive_tlp_count_ < max_tail_loss_probes_) {
    if (unacked_packets_.HasUnackedRetransmittableFrames()) {
      return TLP_MODE;
    }
  }
  return RTO_MODE;
}

void QuicSentPacketManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame,
    const QuicTime& feedback_receive_time) {
  send_algorithm_->OnIncomingQuicCongestionFeedbackFrame(
      frame, feedback_receive_time);
}

void QuicSentPacketManager::InvokeLossDetection(QuicTime time) {
  SequenceNumberSet lost_packets =
      loss_algorithm_->DetectLostPackets(unacked_packets_,
                                         time,
                                         largest_observed_,
                                         rtt_stats_);
  for (SequenceNumberSet::const_iterator it = lost_packets.begin();
       it != lost_packets.end(); ++it) {
    QuicPacketSequenceNumber sequence_number = *it;
    const TransmissionInfo& transmission_info =
        unacked_packets_.GetTransmissionInfo(sequence_number);
    // TODO(ianswett): If it's expected the FEC packet may repair the loss, it
    // should be recorded as a loss to the send algorithm, but not retransmitted
    // until it's known whether the FEC packet arrived.
    ++stats_->packets_lost;
    packets_lost_[sequence_number] = transmission_info;
    DVLOG(1) << ENDPOINT << "Lost packet " << sequence_number;

    if (transmission_info.retransmittable_frames != NULL) {
      MarkForRetransmission(sequence_number, LOSS_RETRANSMISSION);
    } else {
      // Since we will not retransmit this, we need to remove it from
      // unacked_packets_.   This is either the current transmission of
      // a packet whose previous transmission has been acked, a packet that has
      // been TLP retransmitted, or an FEC packet.
      unacked_packets_.RemoveFromInFlight(sequence_number);
    }
  }
}

bool QuicSentPacketManager::MaybeUpdateRTT(
    const ReceivedPacketInfo& received_info,
    const QuicTime& ack_receive_time) {
  if (!unacked_packets_.IsUnacked(received_info.largest_observed)) {
    return false;
  }
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  const TransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(received_info.largest_observed);
  // Don't update the RTT if it hasn't been sent.
  if (transmission_info.sent_time == QuicTime::Zero()) {
    return false;
  }

  QuicTime::Delta send_delta =
      ack_receive_time.Subtract(transmission_info.sent_time);
  rtt_stats_.UpdateRtt(
      send_delta, received_info.delta_time_largest_observed, ack_receive_time);
  return true;
}

QuicTime::Delta QuicSentPacketManager::TimeUntilSend(
    QuicTime now,
    HasRetransmittableData retransmittable) {
  // The TLP logic is entirely contained within QuicSentPacketManager, so the
  // send algorithm does not need to be consulted.
  if (pending_tlp_transmission_) {
    return QuicTime::Delta::Zero();
  }
  return send_algorithm_->TimeUntilSend(
      now, unacked_packets_.bytes_in_flight(), retransmittable);
}

// Ensures that the Delayed Ack timer is always set to a value lesser
// than the retransmission timer's minimum value (MinRTO). We want the
// delayed ack to get back to the QUIC peer before the sender's
// retransmission timer triggers.  Since we do not know the
// reverse-path one-way delay, we assume equal delays for forward and
// reverse paths, and ensure that the timer is set to less than half
// of the MinRTO.
// There may be a value in making this delay adaptive with the help of
// the sender and a signaling mechanism -- if the sender uses a
// different MinRTO, we may get spurious retransmissions. May not have
// any benefits, but if the delayed ack becomes a significant source
// of (likely, tail) latency, then consider such a mechanism.
const QuicTime::Delta QuicSentPacketManager::DelayedAckTime() const {
  return QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs/2);
}

const QuicTime QuicSentPacketManager::GetRetransmissionTime() const {
  // Don't set the timer if there are no packets in flight or we've already
  // queued a tlp transmission and it hasn't been sent yet.
  if (!unacked_packets_.HasInFlightPackets() || pending_tlp_transmission_) {
    return QuicTime::Zero();
  }
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      return clock_->ApproximateNow().Add(GetCryptoRetransmissionDelay());
    case LOSS_MODE:
      return loss_algorithm_->GetLossTimeout();
    case TLP_MODE: {
      // TODO(ianswett): When CWND is available, it would be preferable to
      // set the timer based on the earliest retransmittable packet.
      // Base the updated timer on the send time of the last packet.
      const QuicTime sent_time = unacked_packets_.GetLastPacketSentTime();
      const QuicTime tlp_time = sent_time.Add(GetTailLossProbeDelay());
      // Ensure the TLP timer never gets set to a time in the past.
      return QuicTime::Max(clock_->ApproximateNow(), tlp_time);
    }
    case RTO_MODE: {
      // The RTO is based on the first outstanding packet.
      const QuicTime sent_time =
          unacked_packets_.GetFirstInFlightPacketSentTime();
      QuicTime rto_timeout = sent_time.Add(GetRetransmissionDelay());
      // Always wait at least 1.5 * RTT from now.
      QuicTime min_timeout = clock_->ApproximateNow().Add(
          rtt_stats_.SmoothedRtt().Multiply(1.5));

      return QuicTime::Max(min_timeout, rto_timeout);
    }
  }
  DCHECK(false);
  return QuicTime::Zero();
}

const QuicTime::Delta QuicSentPacketManager::GetCryptoRetransmissionDelay()
    const {
  // This is equivalent to the TailLossProbeDelay, but slightly more aggressive
  // because crypto handshake messages don't incur a delayed ack time.
  int64 delay_ms = max<int64>(kMinHandshakeTimeoutMs,
                              1.5 * rtt_stats_.SmoothedRtt().ToMilliseconds());
  return QuicTime::Delta::FromMilliseconds(
      delay_ms << consecutive_crypto_retransmission_count_);
}

const QuicTime::Delta QuicSentPacketManager::GetTailLossProbeDelay() const {
  QuicTime::Delta srtt = rtt_stats_.SmoothedRtt();
  if (!unacked_packets_.HasMultipleInFlightPackets()) {
    return QuicTime::Delta::Max(
        srtt.Multiply(1.5).Add(DelayedAckTime()), srtt.Multiply(2));
  }
  return QuicTime::Delta::FromMilliseconds(
      max(kMinTailLossProbeTimeoutMs,
          static_cast<int64>(2 * srtt.ToMilliseconds())));
}

const QuicTime::Delta QuicSentPacketManager::GetRetransmissionDelay() const {
  QuicTime::Delta retransmission_delay = send_algorithm_->RetransmissionDelay();
  // TODO(rch): This code should move to |send_algorithm_|.
  if (retransmission_delay.IsZero()) {
    // We are in the initial state, use default timeout values.
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  } else if (retransmission_delay.ToMilliseconds() < kMinRetransmissionTimeMs) {
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs);
  }

  // Calculate exponential back off.
  retransmission_delay = retransmission_delay.Multiply(
      1 << min<size_t>(consecutive_rto_count_, kMaxRetransmissions));

  if (retransmission_delay.ToMilliseconds() > kMaxRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMaxRetransmissionTimeMs);
  }
  return retransmission_delay;
}

const RttStats* QuicSentPacketManager::GetRttStats() const {
  return &rtt_stats_;
}

QuicBandwidth QuicSentPacketManager::BandwidthEstimate() const {
  return send_algorithm_->BandwidthEstimate();
}

QuicByteCount QuicSentPacketManager::GetCongestionWindow() const {
  return send_algorithm_->GetCongestionWindow();
}

void QuicSentPacketManager::MaybeEnablePacing() {
  if (!FLAGS_enable_quic_pacing) {
    return;
  }

  if (using_pacing_) {
    return;
  }

  // Set up a pacing sender with a 5 millisecond alarm granularity.
  using_pacing_ = true;
  send_algorithm_.reset(
      new PacingSender(send_algorithm_.release(),
                       QuicTime::Delta::FromMilliseconds(5)));
}

}  // namespace net
