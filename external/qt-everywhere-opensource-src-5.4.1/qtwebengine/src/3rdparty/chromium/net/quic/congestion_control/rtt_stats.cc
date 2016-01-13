// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/rtt_stats.h"

namespace net {

namespace {

// Default initial rtt used before any samples are received.
const int kInitialRttMs = 100;
const float kAlpha = 0.125f;
const float kOneMinusAlpha = (1 - kAlpha);
const float kBeta = 0.25f;
const float kOneMinusBeta = (1 - kBeta);
const float kHalfWindow = 0.5f;
const float kQuarterWindow = 0.25f;

}  // namespace

RttStats::RttStats()
    : latest_rtt_(QuicTime::Delta::Zero()),
      min_rtt_(QuicTime::Delta::Zero()),
      smoothed_rtt_(QuicTime::Delta::Zero()),
      mean_deviation_(QuicTime::Delta::Zero()),
      initial_rtt_us_(kInitialRttMs * base::Time::kMicrosecondsPerMillisecond),
      num_min_rtt_samples_remaining_(0),
      recent_min_rtt_window_(QuicTime::Delta::Infinite()) {}

bool RttStats::HasUpdates() const {
  return !smoothed_rtt_.IsZero();
}

void RttStats::SampleNewRecentMinRtt(uint32 num_samples) {
  num_min_rtt_samples_remaining_ = num_samples;
  new_min_rtt_ = RttSample();
}

// Updates the RTT based on a new sample.
void RttStats::UpdateRtt(QuicTime::Delta send_delta,
                         QuicTime::Delta ack_delay,
                         QuicTime now) {
  QuicTime::Delta rtt_sample(QuicTime::Delta::Zero());
  if (send_delta > ack_delay) {
    rtt_sample = send_delta.Subtract(ack_delay);
  } else if (!HasUpdates()) {
    // Even though we received information from the peer suggesting
    // an invalid (negative) RTT, we can use the send delta as an
    // approximation until we get a better estimate.
    rtt_sample = send_delta;
  }

  if (rtt_sample.IsInfinite() || rtt_sample.IsZero()) {
    DVLOG(1) << "Ignoring rtt, because it's "
             << (rtt_sample.IsZero() ? "Zero" : "Infinite");
    return;
  }
  // RTT can't be negative.
  DCHECK_LT(0, rtt_sample.ToMicroseconds());

  latest_rtt_ = rtt_sample;
  // First time call or link delay decreases.
  if (min_rtt_.IsZero() || min_rtt_ > rtt_sample) {
    min_rtt_ = rtt_sample;
  }
  UpdateRecentMinRtt(rtt_sample, now);
  // First time call.
  if (!HasUpdates()) {
    smoothed_rtt_ = rtt_sample;
    mean_deviation_ = QuicTime::Delta::FromMicroseconds(
        rtt_sample.ToMicroseconds() / 2);
  } else {
    mean_deviation_ = QuicTime::Delta::FromMicroseconds(
        kOneMinusBeta * mean_deviation_.ToMicroseconds() +
        kBeta * std::abs(smoothed_rtt_.Subtract(rtt_sample).ToMicroseconds()));
    smoothed_rtt_ = smoothed_rtt_.Multiply(kOneMinusAlpha).Add(
        rtt_sample.Multiply(kAlpha));
    DVLOG(1) << "Cubic; smoothed_rtt(us):" << smoothed_rtt_.ToMicroseconds()
             << " mean_deviation(us):" << mean_deviation_.ToMicroseconds();
  }
}

void RttStats::UpdateRecentMinRtt(QuicTime::Delta rtt_sample, QuicTime now) {
  // Recent min_rtt update.
  if (num_min_rtt_samples_remaining_ > 0) {
    --num_min_rtt_samples_remaining_;
    if (new_min_rtt_.rtt.IsZero() || rtt_sample <= new_min_rtt_.rtt) {
      new_min_rtt_ = RttSample(rtt_sample, now);
    }
    if (num_min_rtt_samples_remaining_ == 0) {
      quarter_window_rtt_ = half_window_rtt_ = recent_min_rtt_ = new_min_rtt_;
    }
  }

  // Update the three recent rtt samples.
  if (recent_min_rtt_.rtt.IsZero() || rtt_sample <= recent_min_rtt_.rtt) {
    recent_min_rtt_ = RttSample(rtt_sample, now);
    quarter_window_rtt_ = half_window_rtt_ = recent_min_rtt_;
  } else if (rtt_sample <= half_window_rtt_.rtt) {
    half_window_rtt_ = RttSample(rtt_sample, now);
    quarter_window_rtt_ = half_window_rtt_;
  } else if (rtt_sample <= quarter_window_rtt_.rtt) {
    quarter_window_rtt_ = RttSample(rtt_sample, now);
  }

  // Expire old min rtt samples.
  if (recent_min_rtt_.time < now.Subtract(recent_min_rtt_window_)) {
    recent_min_rtt_ = half_window_rtt_;
    half_window_rtt_ = quarter_window_rtt_;
    quarter_window_rtt_ = RttSample(rtt_sample, now);
  } else if (half_window_rtt_.time <
      now.Subtract(recent_min_rtt_window_.Multiply(kHalfWindow))) {
    half_window_rtt_ = quarter_window_rtt_;
    quarter_window_rtt_ = RttSample(rtt_sample, now);
  } else if (quarter_window_rtt_.time <
      now.Subtract(recent_min_rtt_window_.Multiply(kQuarterWindow))) {
    quarter_window_rtt_ = RttSample(rtt_sample, now);
  }
}

QuicTime::Delta RttStats::SmoothedRtt() const {
  if (!HasUpdates()) {
    return QuicTime::Delta::FromMicroseconds(initial_rtt_us_);
  }
  return smoothed_rtt_;
}

}  // namespace net
