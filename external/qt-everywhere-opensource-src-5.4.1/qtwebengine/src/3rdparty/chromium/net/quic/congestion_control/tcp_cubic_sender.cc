// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/tcp_cubic_sender.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "net/quic/congestion_control/rtt_stats.h"

using std::max;
using std::min;

namespace net {

namespace {
// Constants based on TCP defaults.
// The minimum cwnd based on RFC 3782 (TCP NewReno) for cwnd reductions on a
// fast retransmission.  The cwnd after a timeout is still 1.
const QuicTcpCongestionWindow kMinimumCongestionWindow = 2;
const QuicByteCount kMaxSegmentSize = kDefaultTCPMSS;
const QuicByteCount kDefaultReceiveWindow = 64000;
const int64 kInitialCongestionWindow = 10;
const int kMaxBurstLength = 3;
};  // namespace

TcpCubicSender::TcpCubicSender(
    const QuicClock* clock,
    const RttStats* rtt_stats,
    bool reno,
    QuicTcpCongestionWindow max_tcp_congestion_window,
    QuicConnectionStats* stats)
    : hybrid_slow_start_(clock),
      cubic_(clock, stats),
      rtt_stats_(rtt_stats),
      stats_(stats),
      reno_(reno),
      congestion_window_count_(0),
      receive_window_(kDefaultReceiveWindow),
      prr_out_(0),
      prr_delivered_(0),
      ack_count_since_loss_(0),
      bytes_in_flight_before_loss_(0),
      largest_sent_sequence_number_(0),
      largest_acked_sequence_number_(0),
      largest_sent_at_last_cutback_(0),
      congestion_window_(kInitialCongestionWindow),
      slowstart_threshold_(max_tcp_congestion_window),
      last_cutback_exited_slowstart_(false),
      max_tcp_congestion_window_(max_tcp_congestion_window) {
}

TcpCubicSender::~TcpCubicSender() {
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.FinalTcpCwnd", congestion_window_);
}

bool TcpCubicSender::InSlowStart() const {
  return congestion_window_ < slowstart_threshold_;
}

void TcpCubicSender::SetFromConfig(const QuicConfig& config, bool is_server) {
  if (is_server && config.HasReceivedInitialCongestionWindow()) {
    // Set the initial window size.
    congestion_window_ = min(kMaxInitialWindow,
                             config.ReceivedInitialCongestionWindow());
  }
}

void TcpCubicSender::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback,
    QuicTime feedback_receive_time) {
  receive_window_ = feedback.tcp.receive_window;
}

void TcpCubicSender::OnCongestionEvent(
    bool rtt_updated,
    QuicByteCount bytes_in_flight,
    const CongestionMap& acked_packets,
    const CongestionMap& lost_packets) {
  if (rtt_updated && InSlowStart() &&
      hybrid_slow_start_.ShouldExitSlowStart(rtt_stats_->latest_rtt(),
                                             rtt_stats_->min_rtt(),
                                             congestion_window_)) {
    slowstart_threshold_ = congestion_window_;
  }
  for (CongestionMap::const_iterator it = lost_packets.begin();
       it != lost_packets.end(); ++it) {
    OnPacketLost(it->first, bytes_in_flight);
  }
  for (CongestionMap::const_iterator it = acked_packets.begin();
       it != acked_packets.end(); ++it) {
    OnPacketAcked(it->first, it->second.bytes_sent, bytes_in_flight);
  }
}

void TcpCubicSender::OnPacketAcked(
    QuicPacketSequenceNumber acked_sequence_number,
    QuicByteCount acked_bytes,
    QuicByteCount bytes_in_flight) {
  largest_acked_sequence_number_ = max(acked_sequence_number,
                                       largest_acked_sequence_number_);
  if (InRecovery()) {
    PrrOnPacketAcked(acked_bytes);
    return;
  }
  MaybeIncreaseCwnd(acked_sequence_number, bytes_in_flight);
  // TODO(ianswett): Should this even be called when not in slow start?
  hybrid_slow_start_.OnPacketAcked(acked_sequence_number, InSlowStart());
}

void TcpCubicSender::OnPacketLost(QuicPacketSequenceNumber sequence_number,
                                  QuicByteCount bytes_in_flight) {
  // TCP NewReno (RFC6582) says that once a loss occurs, any losses in packets
  // already sent should be treated as a single loss event, since it's expected.
  if (sequence_number <= largest_sent_at_last_cutback_) {
    if (last_cutback_exited_slowstart_) {
      ++stats_->slowstart_packets_lost;
    }
    DVLOG(1) << "Ignoring loss for largest_missing:" << sequence_number
             << " because it was sent prior to the last CWND cutback.";
    return;
  }
  ++stats_->tcp_loss_events;
  last_cutback_exited_slowstart_ = InSlowStart();
  if (InSlowStart()) {
    ++stats_->slowstart_packets_lost;
  }
  PrrOnPacketLost(bytes_in_flight);

  if (reno_) {
    congestion_window_ = congestion_window_ >> 1;
  } else {
    congestion_window_ =
        cubic_.CongestionWindowAfterPacketLoss(congestion_window_);
  }
  slowstart_threshold_ = congestion_window_;
  // Enforce TCP's minimum congestion window of 2*MSS.
  if (congestion_window_ < kMinimumCongestionWindow) {
    congestion_window_ = kMinimumCongestionWindow;
  }
  largest_sent_at_last_cutback_ = largest_sent_sequence_number_;
  // reset packet count from congestion avoidance mode. We start
  // counting again when we're out of recovery.
  congestion_window_count_ = 0;
  DVLOG(1) << "Incoming loss; congestion window: " << congestion_window_
           << " slowstart threshold: " << slowstart_threshold_;
}

bool TcpCubicSender::OnPacketSent(QuicTime /*sent_time*/,
                                  QuicByteCount /*bytes_in_flight*/,
                                  QuicPacketSequenceNumber sequence_number,
                                  QuicByteCount bytes,
                                  HasRetransmittableData is_retransmittable) {
  // Only update bytes_in_flight_ for data packets.
  if (is_retransmittable != HAS_RETRANSMITTABLE_DATA) {
    return false;
  }

  prr_out_ += bytes;
  if (largest_sent_sequence_number_ < sequence_number) {
    // TODO(rch): Ensure that packets are really sent in order.
    // DCHECK_LT(largest_sent_sequence_number_, sequence_number);
    largest_sent_sequence_number_ = sequence_number;
  }
  hybrid_slow_start_.OnPacketSent(sequence_number);
  return true;
}

QuicTime::Delta TcpCubicSender::TimeUntilSend(
    QuicTime /* now */,
    QuicByteCount bytes_in_flight,
    HasRetransmittableData has_retransmittable_data) const {
  if (has_retransmittable_data == NO_RETRANSMITTABLE_DATA) {
    // For TCP we can always send an ACK immediately.
    return QuicTime::Delta::Zero();
  }
  if (InRecovery()) {
    return PrrTimeUntilSend(bytes_in_flight);
  }
  if (SendWindow() > bytes_in_flight) {
    return QuicTime::Delta::Zero();
  }
  return QuicTime::Delta::Infinite();
}

QuicByteCount TcpCubicSender::SendWindow() const {
  // What's the current send window in bytes.
  return min(receive_window_, GetCongestionWindow());
}

QuicBandwidth TcpCubicSender::BandwidthEstimate() const {
  return QuicBandwidth::FromBytesAndTimeDelta(GetCongestionWindow(),
                                              rtt_stats_->SmoothedRtt());
}

QuicTime::Delta TcpCubicSender::RetransmissionDelay() const {
  if (!rtt_stats_->HasUpdates()) {
    return QuicTime::Delta::Zero();
  }
  return QuicTime::Delta::FromMicroseconds(
      rtt_stats_->SmoothedRtt().ToMicroseconds() +
      4 * rtt_stats_->mean_deviation().ToMicroseconds());
}

QuicByteCount TcpCubicSender::GetCongestionWindow() const {
  return congestion_window_ * kMaxSegmentSize;
}

bool TcpCubicSender::IsCwndLimited(QuicByteCount bytes_in_flight) const {
  const QuicByteCount congestion_window_bytes = congestion_window_ *
      kMaxSegmentSize;
  if (bytes_in_flight >= congestion_window_bytes) {
    return true;
  }
  const QuicByteCount max_burst = kMaxBurstLength * kMaxSegmentSize;
  const QuicByteCount available_bytes =
      congestion_window_bytes - bytes_in_flight;
  const bool slow_start_limited = InSlowStart() &&
      bytes_in_flight >  congestion_window_bytes / 2;
  return slow_start_limited || available_bytes <= max_burst;
}

bool TcpCubicSender::InRecovery() const {
  return largest_acked_sequence_number_ <= largest_sent_at_last_cutback_ &&
      largest_acked_sequence_number_ != 0;
}

// Called when we receive an ack. Normal TCP tracks how many packets one ack
// represents, but quic has a separate ack for each packet.
void TcpCubicSender::MaybeIncreaseCwnd(
    QuicPacketSequenceNumber acked_sequence_number,
    QuicByteCount bytes_in_flight) {
  LOG_IF(DFATAL, InRecovery()) << "Never increase the CWND during recovery.";
  if (!IsCwndLimited(bytes_in_flight)) {
    // We don't update the congestion window unless we are close to using the
    // window we have available.
    return;
  }
  if (InSlowStart()) {
    // congestion_window_cnt is the number of acks since last change of snd_cwnd
    if (congestion_window_ < max_tcp_congestion_window_) {
      // TCP slow start, exponential growth, increase by one for each ACK.
      ++congestion_window_;
    }
    DVLOG(1) << "Slow start; congestion window: " << congestion_window_
             << " slowstart threshold: " << slowstart_threshold_;
    return;
  }
  if (congestion_window_ >= max_tcp_congestion_window_) {
    return;
  }
  // Congestion avoidance
  if (reno_) {
    // Classic Reno congestion avoidance provided for testing.

    ++congestion_window_count_;
    if (congestion_window_count_ >= congestion_window_) {
      ++congestion_window_;
      congestion_window_count_ = 0;
    }

    DVLOG(1) << "Reno; congestion window: " << congestion_window_
             << " slowstart threshold: " << slowstart_threshold_
             << " congestion window count: " << congestion_window_count_;
  } else {
    congestion_window_ = min(max_tcp_congestion_window_,
                             cubic_.CongestionWindowAfterAck(
                                 congestion_window_, rtt_stats_->min_rtt()));
    DVLOG(1) << "Cubic; congestion window: " << congestion_window_
             << " slowstart threshold: " << slowstart_threshold_;
  }
}

void TcpCubicSender::OnRetransmissionTimeout(bool packets_retransmitted) {
  largest_sent_at_last_cutback_ = 0;
  if (packets_retransmitted) {
    cubic_.Reset();
    hybrid_slow_start_.Restart();
    congestion_window_ = kMinimumCongestionWindow;
  }
}

void TcpCubicSender::PrrOnPacketLost(QuicByteCount bytes_in_flight) {
  prr_out_ = 0;
  bytes_in_flight_before_loss_ = bytes_in_flight;
  prr_delivered_ = 0;
  ack_count_since_loss_ = 0;
}

void TcpCubicSender::PrrOnPacketAcked(QuicByteCount acked_bytes) {
  prr_delivered_ += acked_bytes;
  ++ack_count_since_loss_;
}

QuicTime::Delta TcpCubicSender::PrrTimeUntilSend(
    QuicByteCount bytes_in_flight) const {
  DCHECK(InRecovery());
  // Return QuicTime::Zero In order to ensure limited transmit always works.
  if (prr_out_ == 0) {
    return QuicTime::Delta::Zero();
  }
  if (SendWindow() > bytes_in_flight) {
    // During PRR-SSRB, limit outgoing packets to 1 extra MSS per ack, instead
    // of sending the entire available window. This prevents burst retransmits
    // when more packets are lost than the CWND reduction.
    //   limit = MAX(prr_delivered - prr_out, DeliveredData) + MSS
    if (prr_delivered_ + ack_count_since_loss_ * kMaxSegmentSize <= prr_out_) {
      return QuicTime::Delta::Infinite();
    }
    return QuicTime::Delta::Zero();
  }
  // Implement Proportional Rate Reduction (RFC6937)
  // Checks a simplified version of the PRR formula that doesn't use division:
  // AvailableSendWindow =
  //   CEIL(prr_delivered * ssthresh / BytesInFlightAtLoss) - prr_sent
  if (prr_delivered_ * slowstart_threshold_ * kMaxSegmentSize >
          prr_out_ * bytes_in_flight_before_loss_) {
    return QuicTime::Delta::Zero();
  }
  return QuicTime::Delta::Infinite();
}

}  // namespace net
