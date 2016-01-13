// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/fix_rate_sender.h"

#include <math.h>

#include <algorithm>

#include "base/logging.h"
#include "net/quic/congestion_control/rtt_stats.h"
#include "net/quic/quic_protocol.h"

using std::max;

namespace {
  const int kInitialBitrate = 100000;  // In bytes per second.
  const uint64 kWindowSizeUs = 10000;  // 10 ms.
}

namespace net {

FixRateSender::FixRateSender(const RttStats* rtt_stats)
    : rtt_stats_(rtt_stats),
      bitrate_(QuicBandwidth::FromBytesPerSecond(kInitialBitrate)),
      max_segment_size_(kDefaultMaxPacketSize),
      fix_rate_leaky_bucket_(bitrate_),
      latest_rtt_(QuicTime::Delta::Zero()) {
  DVLOG(1) << "FixRateSender";
}

FixRateSender::~FixRateSender() {
}

void FixRateSender::SetFromConfig(const QuicConfig& config, bool is_server) {
}

void FixRateSender::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback,
    QuicTime feedback_receive_time) {
  LOG_IF(DFATAL, feedback.type != kFixRate) <<
      "Invalid incoming CongestionFeedbackType:" << feedback.type;
  if (feedback.type == kFixRate) {
    bitrate_ = feedback.fix_rate.bitrate;
    fix_rate_leaky_bucket_.SetDrainingRate(feedback_receive_time, bitrate_);
  }
  // Silently ignore invalid messages in release mode.
}

void FixRateSender::OnCongestionEvent(bool rtt_updated,
                                      QuicByteCount bytes_in_flight,
                                      const CongestionMap& acked_packets,
                                      const CongestionMap& lost_packets) {
}

bool FixRateSender::OnPacketSent(
    QuicTime sent_time,
    QuicByteCount /*bytes_in_flight*/,
    QuicPacketSequenceNumber /*sequence_number*/,
    QuicByteCount bytes,
    HasRetransmittableData /*has_retransmittable_data*/) {
  fix_rate_leaky_bucket_.Add(sent_time, bytes);

  return true;
}

void FixRateSender::OnRetransmissionTimeout(bool packets_retransmitted) { }

QuicTime::Delta FixRateSender::TimeUntilSend(
    QuicTime now,
    QuicByteCount /*bytes_in_flight*/,
    HasRetransmittableData /*has_retransmittable_data*/) const {
  return fix_rate_leaky_bucket_.TimeRemaining(now);
}

QuicByteCount FixRateSender::CongestionWindow() const {
  QuicByteCount window_size_bytes = bitrate_.ToBytesPerPeriod(
      QuicTime::Delta::FromMicroseconds(kWindowSizeUs));
  // Make sure window size is not less than a packet.
  return max(kDefaultMaxPacketSize, window_size_bytes);
}

QuicBandwidth FixRateSender::BandwidthEstimate() const {
  return bitrate_;
}


QuicTime::Delta FixRateSender::RetransmissionDelay() const {
  // TODO(pwestin): Calculate and return retransmission delay.
  // Use 2 * the latest RTT for now.
  return latest_rtt_.Add(latest_rtt_);
}

QuicByteCount FixRateSender::GetCongestionWindow() const {
  return 0;
}

}  // namespace net
