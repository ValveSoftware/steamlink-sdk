// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CONGESTION_CONTROL_CONGESTION_CONTROL_H_
#define MEDIA_CAST_CONGESTION_CONTROL_CONGESTION_CONTROL_H_

#include <deque>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace media {
namespace cast {

class CongestionControl {
 public:
  CongestionControl(base::TickClock* clock,
                    uint32 max_bitrate_configured,
                    uint32 min_bitrate_configured,
                    size_t max_unacked_frames);

  virtual ~CongestionControl();

  void UpdateRtt(base::TimeDelta rtt);

  // Called when an encoded frame is sent to the transport.
  void SendFrameToTransport(uint32 frame_id,
                            size_t frame_size,
                            base::TimeTicks when);

  // Called when we receive an ACK for a frame.
  void AckFrame(uint32 frame_id, base::TimeTicks when);

  // Returns the bitrate we should use for the next frame.
  uint32 GetBitrate(base::TimeTicks playout_time,
                    base::TimeDelta playout_delay);

 private:
  struct FrameStats {
    FrameStats();
    // Time this frame was sent to the transport.
    base::TimeTicks sent_time;
    // Time this frame was acked.
    base::TimeTicks ack_time;
    // Size of encoded frame in bits.
    size_t frame_size;
  };

  // Calculate how much "dead air" (idle time) there is between two frames.
  static base::TimeDelta DeadTime(const FrameStats& a, const FrameStats& b);
  // Get the FrameStats for a given |frame_id|.
  // Note: Older FrameStats will be removed automatically.
  FrameStats* GetFrameStats(uint32 frame_id);
  // Calculata safe bitrate. This is based on how much we've been
  // sending in the past.
  double CalculateSafeBitrate();

  // For a given frame, calculate when it might be acked.
  // (Or return the time it was acked, if it was.)
  base::TimeTicks EstimatedAckTime(uint32 frame_id, double bitrate);
  // Calculate when we start sending the data for a given frame.
  // This is done by calculating when we were done sending the previous
  // frame, but obvoiusly can't be less than |sent_time| (if known).
  base::TimeTicks EstimatedSendingTime(uint32 frame_id, double bitrate);

  base::TickClock* const clock_;  // Not owned by this class.
  const uint32 max_bitrate_configured_;
  const uint32 min_bitrate_configured_;
  std::deque<FrameStats> frame_stats_;
  uint32 last_frame_stats_;
  uint32 last_acked_frame_;
  uint32 last_encoded_frame_;
  base::TimeDelta rtt_;
  size_t history_size_;
  size_t acked_bits_in_history_;
  base::TimeDelta dead_time_in_history_;

  DISALLOW_COPY_AND_ASSIGN(CongestionControl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CONGESTION_CONTROL_CONGESTION_CONTROL_H_
