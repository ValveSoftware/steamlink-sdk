// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of this file is determine what bitrate to use for mirroring.
// Ideally this should be as much as possible, without causing any frames to
// arrive late.

// The current algorithm is to measure how much bandwidth we've been using
// recently. We also keep track of how much data has been queued up for sending
// in a virtual "buffer" (this virtual buffer represents all the buffers between
// the sender and the receiver, including retransmissions and so forth.)
// If we estimate that our virtual buffer is mostly empty, we try to use
// more bandwidth than our recent usage, otherwise we use less.

#include "media/cast/congestion_control/congestion_control.h"

#include "base/logging.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"

namespace media {
namespace cast {

// This means that we *try* to keep our buffer 90% empty.
// If it is less full, we increase the bandwidth, if it is more
// we decrease the bandwidth. Making this smaller makes the
// congestion control more aggressive.
static const double kTargetEmptyBufferFraction = 0.9;

// This is the size of our history in frames. Larger values makes the
// congestion control adapt slower.
static const size_t kHistorySize = 100;

CongestionControl::FrameStats::FrameStats() : frame_size(0) {
}

CongestionControl::CongestionControl(base::TickClock* clock,
                                     uint32 max_bitrate_configured,
                                     uint32 min_bitrate_configured,
                                     size_t max_unacked_frames)
    : clock_(clock),
      max_bitrate_configured_(max_bitrate_configured),
      min_bitrate_configured_(min_bitrate_configured),
      last_frame_stats_(static_cast<uint32>(-1)),
      last_acked_frame_(static_cast<uint32>(-1)),
      last_encoded_frame_(static_cast<uint32>(-1)),
      history_size_(max_unacked_frames + kHistorySize),
      acked_bits_in_history_(0) {
  DCHECK_GE(max_bitrate_configured, min_bitrate_configured) << "Invalid config";
  frame_stats_.resize(2);
  base::TimeTicks now = clock->NowTicks();
  frame_stats_[0].ack_time = now;
  frame_stats_[0].sent_time = now;
  frame_stats_[1].ack_time = now;
  DCHECK(!frame_stats_[0].ack_time.is_null());
}

CongestionControl::~CongestionControl() {}

void CongestionControl::UpdateRtt(base::TimeDelta rtt) {
  rtt_ = base::TimeDelta::FromSecondsD(
      (rtt_.InSecondsF() * 7 + rtt.InSecondsF()) / 8);
}

// Calculate how much "dead air" there is between two frames.
base::TimeDelta CongestionControl::DeadTime(const FrameStats& a,
                                            const FrameStats& b) {
  if (b.sent_time > a.ack_time) {
    return b.sent_time - a.ack_time;
  } else {
    return base::TimeDelta();
  }
}

double CongestionControl::CalculateSafeBitrate() {
  double transmit_time =
      (GetFrameStats(last_acked_frame_)->ack_time -
       frame_stats_.front().sent_time - dead_time_in_history_).InSecondsF();

  if (acked_bits_in_history_ == 0 || transmit_time <= 0.0) {
    return min_bitrate_configured_;
  }
  return acked_bits_in_history_ / std::max(transmit_time, 1E-3);
}

CongestionControl::FrameStats* CongestionControl::GetFrameStats(
    uint32 frame_id) {
  int32 offset = static_cast<int32>(frame_id - last_frame_stats_);
  DCHECK_LT(offset, static_cast<int32>(kHistorySize));
  if (offset > 0) {
    frame_stats_.resize(frame_stats_.size() + offset);
    last_frame_stats_ += offset;
    offset = 0;
  }
  while (frame_stats_.size() > history_size_) {
    DCHECK_GT(frame_stats_.size(), 1UL);
    DCHECK(!frame_stats_[0].ack_time.is_null());
    acked_bits_in_history_ -= frame_stats_[0].frame_size;
    dead_time_in_history_ -= DeadTime(frame_stats_[0], frame_stats_[1]);
    DCHECK_GE(acked_bits_in_history_, 0UL);
    VLOG(2) << "DT: " << dead_time_in_history_.InSecondsF();
    DCHECK_GE(dead_time_in_history_.InSecondsF(), 0.0);
    frame_stats_.pop_front();
  }
  offset += frame_stats_.size() - 1;
  if (offset < 0 || offset >= static_cast<int32>(frame_stats_.size())) {
    return NULL;
  }
  return &frame_stats_[offset];
}

void CongestionControl::AckFrame(uint32 frame_id, base::TimeTicks when) {
  FrameStats* frame_stats = GetFrameStats(last_acked_frame_);
  while (IsNewerFrameId(frame_id, last_acked_frame_)) {
    FrameStats* last_frame_stats = frame_stats;
    last_acked_frame_++;
    frame_stats = GetFrameStats(last_acked_frame_);
    DCHECK(frame_stats);
    frame_stats->ack_time = when;
    acked_bits_in_history_ += frame_stats->frame_size;
    dead_time_in_history_ += DeadTime(*last_frame_stats, *frame_stats);
  }
}

void CongestionControl::SendFrameToTransport(uint32 frame_id,
                                             size_t frame_size,
                                             base::TimeTicks when) {
  last_encoded_frame_ = frame_id;
  FrameStats* frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  frame_stats->frame_size = frame_size;
  frame_stats->sent_time = when;
}

base::TimeTicks CongestionControl::EstimatedAckTime(uint32 frame_id,
                                                    double bitrate) {
  FrameStats* frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  if (frame_stats->ack_time.is_null()) {
    DCHECK(frame_stats->frame_size) << "frame_id: " << frame_id;
    base::TimeTicks ret = EstimatedSendingTime(frame_id, bitrate);
    ret += base::TimeDelta::FromSecondsD(frame_stats->frame_size / bitrate);
    ret += rtt_;
    base::TimeTicks now = clock_->NowTicks();
    if (ret < now) {
      // This is a little counter-intuitive, but it seems to work.
      // Basically, when we estimate that the ACK should have already happened,
      // we figure out how long ago it should have happened and guess that the
      // ACK will happen half of that time in the future. This will cause some
      // over-estimation when acks are late, which is actually what we want.
      return now + (now - ret) / 2;
    } else {
      return ret;
    }
  } else {
    return frame_stats->ack_time;
  }
}

base::TimeTicks CongestionControl::EstimatedSendingTime(uint32 frame_id,
                                                        double bitrate) {
  FrameStats* frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  base::TimeTicks ret = EstimatedAckTime(frame_id - 1, bitrate) - rtt_;
  if (frame_stats->sent_time.is_null()) {
    // Not sent yet, but we can't start sending it in the past.
    return std::max(ret, clock_->NowTicks());
  } else {
    return std::max(ret, frame_stats->sent_time);
  }
}

uint32 CongestionControl::GetBitrate(base::TimeTicks playout_time,
                                     base::TimeDelta playout_delay) {
  double safe_bitrate = CalculateSafeBitrate();
  // Estimate when we might start sending the next frame.
  base::TimeDelta time_to_catch_up =
      playout_time -
      EstimatedSendingTime(last_encoded_frame_ + 1, safe_bitrate);

  double empty_buffer_fraction =
      time_to_catch_up.InSecondsF() / playout_delay.InSecondsF();
  empty_buffer_fraction = std::min(empty_buffer_fraction, 1.0);
  empty_buffer_fraction = std::max(empty_buffer_fraction, 0.0);

  uint32 bits_per_second = static_cast<uint32>(
      safe_bitrate * empty_buffer_fraction / kTargetEmptyBufferFraction);
  VLOG(3) << " FBR:" << (bits_per_second / 1E6)
          << " EBF:" << empty_buffer_fraction
          << " SBR:" << (safe_bitrate / 1E6);
  bits_per_second = std::max(bits_per_second, min_bitrate_configured_);
  bits_per_second = std::min(bits_per_second, max_bitrate_configured_);
  return bits_per_second;
}

}  // namespace cast
}  // namespace media
