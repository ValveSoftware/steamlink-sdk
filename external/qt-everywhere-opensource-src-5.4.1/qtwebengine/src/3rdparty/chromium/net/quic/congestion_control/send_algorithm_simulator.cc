// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/send_algorithm_simulator.h"

#include <limits>

#include "base/logging.h"
#include "base/rand_util.h"
#include "net/quic/crypto/quic_random.h"

using std::list;
using std::max;
using std::min;

namespace net {

namespace {

const QuicByteCount kPacketSize = 1200;

}  // namespace

SendAlgorithmSimulator::SendAlgorithmSimulator(
    SendAlgorithmInterface* send_algorithm,
    MockClock* clock,
    RttStats* rtt_stats,
    QuicBandwidth bandwidth,
    QuicTime::Delta rtt)
    : send_algorithm_(send_algorithm),
      clock_(clock),
      rtt_stats_(rtt_stats),
      next_sent_(1),
      last_acked_(0),
      next_acked_(1),
      lose_next_ack_(false),
      bytes_in_flight_(0),
      forward_loss_rate_(0),
      reverse_loss_rate_(0),
      loss_correlation_(0),
      bandwidth_(bandwidth),
      rtt_(rtt),
      buffer_size_(1000000),
      max_cwnd_(0),
      min_cwnd_(100000),
      max_cwnd_drop_(0),
      last_cwnd_(0) {
  uint32 seed = base::RandInt(0, std::numeric_limits<int32>::max());
  DVLOG(1) << "Seeding SendAlgorithmSimulator with " << seed;
  simple_random_.set_seed(seed);
}

SendAlgorithmSimulator::~SendAlgorithmSimulator() {}

// Sends the specified number of bytes as quickly as possible and returns the
// average bandwidth in bytes per second.  The time elapsed is based on
// waiting for all acks to arrive.
QuicBandwidth SendAlgorithmSimulator::SendBytes(size_t num_bytes) {
  const QuicTime start_time = clock_->Now();
  size_t bytes_acked = 0;
  while (bytes_acked < num_bytes) {
    DVLOG(1) << "bytes_acked:" << bytes_acked << " bytes_in_flight_:"
             << bytes_in_flight_ << " CWND(bytes):"
             << send_algorithm_->GetCongestionWindow();
    // Determine the times of next send and of the next ack arrival.
    QuicTime::Delta send_delta = send_algorithm_->TimeUntilSend(
        clock_->Now(), bytes_in_flight_, HAS_RETRANSMITTABLE_DATA);
    // If we've already sent enough bytes, wait for them to be acked.
    if (bytes_acked + bytes_in_flight_ >= num_bytes) {
      send_delta = QuicTime::Delta::Infinite();
    }
    QuicTime::Delta ack_delta = NextAckDelta();
    // If both times are infinite, fire a TLP.
    if (ack_delta.IsInfinite() && send_delta.IsInfinite()) {
      DVLOG(1) << "Both times are infinite, simulating a TLP.";
      // TODO(ianswett): Use a more sophisticated TLP timer.
      clock_->AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
      SendDataNow();
    } else if (ack_delta < send_delta) {
      DVLOG(1) << "Handling ack, advancing time:"
               << ack_delta.ToMicroseconds() << "us";
      // Ack data all the data up to ack time and lose any missing sequence
      // numbers.
      clock_->AdvanceTime(ack_delta);
      bytes_acked += HandlePendingAck();
    } else {
      DVLOG(1) << "Sending, advancing time:"
               << send_delta.ToMicroseconds() << "us";
      clock_->AdvanceTime(send_delta);
      SendDataNow();
    }
    RecordStats();
  }
  return QuicBandwidth::FromBytesAndTimeDelta(
      num_bytes, clock_->Now().Subtract(start_time));
}

// NextAck takes into account packet loss in both forward and reverse
// direction, as well as correlated losses.  And it assumes the receiver acks
// every other packet when there is no loss.
QuicTime::Delta SendAlgorithmSimulator::NextAckDelta() {
  if (sent_packets_.empty() || AllPacketsLost()) {
    DVLOG(1) << "No outstanding packets to cause acks. sent_packets_.size():"
             << sent_packets_.size();
    return QuicTime::Delta::Infinite();
  }

  // If necessary, determine next_acked_.
  // This is only done once to ensure multiple calls return the same time.
  FindNextAcked();

  // If only one packet is acked, simulate a delayed ack.
  if (next_acked_ - last_acked_ == 1) {
    return sent_packets_.front().ack_time.Add(
        QuicTime::Delta::FromMilliseconds(100)).Subtract(clock_->Now());
  }
  for (list<SentPacket>::const_iterator it = sent_packets_.begin();
       it != sent_packets_.end(); ++it) {
    if (next_acked_ == it->sequence_number) {
      return it->ack_time.Subtract(clock_->Now());
    }
  }
  LOG(DFATAL) << "Error, next_acked_: " << next_acked_
              << " should have been found in sent_packets_";
  return QuicTime::Delta::Infinite();
}

bool SendAlgorithmSimulator::AllPacketsLost() {
  for (list<SentPacket>::const_iterator it = sent_packets_.begin();
       it != sent_packets_.end(); ++it) {
    if (it->ack_time.IsInitialized()) {
      return false;
    }
  }
  return true;
}

void SendAlgorithmSimulator::FindNextAcked() {
  // TODO(ianswett): Add a simpler mode which acks every packet.
  bool packets_lost = false;
  if (next_acked_ == last_acked_) {
    // Determine if the next ack is lost only once, to ensure determinism.
    lose_next_ack_ =
        reverse_loss_rate_ * kuint64max > simple_random_.RandUint64();
  }
  bool two_acks_remaining = lose_next_ack_;
  next_acked_ = last_acked_;
  // Remove any packets that are simulated as lost.
  for (list<SentPacket>::const_iterator it = sent_packets_.begin();
      it != sent_packets_.end(); ++it) {
    // Lost packets don't trigger an ack.
    if (it->ack_time  == QuicTime::Zero()) {
      packets_lost = true;
      continue;
    }
    // Buffer dropped packets are skipped automatically, but still end up
    // being lost and cause acks to be sent immediately.
    if (next_acked_ < it->sequence_number - 1) {
      packets_lost = true;
    }
    next_acked_ = it->sequence_number;
    if (packets_lost || (next_acked_ - last_acked_) % 2 == 0) {
      if (two_acks_remaining) {
        two_acks_remaining = false;
      } else {
        break;
      }
    }
  }
  DVLOG(1) << "FindNextAcked found next_acked_:" << next_acked_
           << " last_acked:" << last_acked_;
}

int SendAlgorithmSimulator::HandlePendingAck() {
  DCHECK_LT(last_acked_, next_acked_);
  SendAlgorithmInterface::CongestionMap acked_packets;
  SendAlgorithmInterface::CongestionMap lost_packets;
  // Some entries may be missing from the sent_packets_ array, if they were
  // dropped due to buffer overruns.
  SentPacket largest_observed = sent_packets_.front();
  while (last_acked_ < next_acked_) {
    ++last_acked_;
    TransmissionInfo info = TransmissionInfo();
    info.bytes_sent = kPacketSize;
    info.in_flight = true;
    // If it's missing from the array, it's a loss.
    if (sent_packets_.front().sequence_number > last_acked_) {
      DVLOG(1) << "Lost packet:" << last_acked_
               << " dropped by buffer overflow.";
      lost_packets[last_acked_] = info;
      continue;
    }
    if (sent_packets_.front().ack_time.IsInitialized()) {
      acked_packets[last_acked_] = info;
    } else {
      lost_packets[last_acked_] = info;
    }
    // Remove all packets from the front to next_acked_.
    largest_observed = sent_packets_.front();
    sent_packets_.pop_front();
  }

  DCHECK(largest_observed.ack_time.IsInitialized());
  rtt_stats_->UpdateRtt(
      largest_observed.ack_time.Subtract(largest_observed.send_time),
      QuicTime::Delta::Zero(),
      clock_->Now());
  send_algorithm_->OnCongestionEvent(
      true, bytes_in_flight_, acked_packets, lost_packets);
  DCHECK_LE(kPacketSize * (acked_packets.size() + lost_packets.size()),
            bytes_in_flight_);
  bytes_in_flight_ -=
      kPacketSize * (acked_packets.size() + lost_packets.size());
  return acked_packets.size() * kPacketSize;
}

void SendAlgorithmSimulator::SendDataNow() {
  DVLOG(1) << "Sending packet:" << next_sent_ << " bytes_in_flight:"
           << bytes_in_flight_;
  send_algorithm_->OnPacketSent(
      clock_->Now(), bytes_in_flight_,
      next_sent_, kPacketSize, HAS_RETRANSMITTABLE_DATA);
  // Lose the packet immediately if the buffer is full.
  if (sent_packets_.size() * kPacketSize < buffer_size_) {
    // TODO(ianswett): This buffer simulation is an approximation.
    // An ack time of zero means loss.
    bool packet_lost =
        forward_loss_rate_ * kuint64max > simple_random_.RandUint64();
    // Handle correlated loss.
    if (!sent_packets_.empty() &&
        !sent_packets_.back().ack_time.IsInitialized() &&
        loss_correlation_ * kuint64max > simple_random_.RandUint64()) {
      packet_lost = true;
    }

    QuicTime ack_time = clock_->Now().Add(rtt_);
    // If the number of bytes in flight are less than the bdp, there's
    // no buffering delay.  Bytes lost from the buffer are not counted.
    QuicByteCount bdp = bandwidth_.ToBytesPerPeriod(rtt_);
    if (sent_packets_.size() * kPacketSize > bdp) {
      QuicByteCount qsize = sent_packets_.size() * kPacketSize - bdp;
      ack_time = ack_time.Add(bandwidth_.TransferTime(qsize));
    }
    // If the packet is lost, give it an ack time of Zero.
    sent_packets_.push_back(SentPacket(
        next_sent_, clock_->Now(), packet_lost ? QuicTime::Zero() : ack_time));
  }
  ++next_sent_;
  bytes_in_flight_ += kPacketSize;
}

void SendAlgorithmSimulator::RecordStats() {
  QuicByteCount cwnd = send_algorithm_->GetCongestionWindow();
  max_cwnd_ = max(max_cwnd_, cwnd);
  min_cwnd_ = min(min_cwnd_, cwnd);
  if (last_cwnd_ > cwnd) {
    max_cwnd_drop_ = max(max_cwnd_drop_, last_cwnd_ - cwnd);
  }
  last_cwnd_ = cwnd;
}

// Advance the time by |delta| without sending anything.
void SendAlgorithmSimulator::AdvanceTime(QuicTime::Delta delta) {
  clock_->AdvanceTime(delta);
}

// Elapsed time from the start of the connection.
QuicTime SendAlgorithmSimulator::ElapsedTime() {
  return clock_->Now();
}

}  // namespace net
