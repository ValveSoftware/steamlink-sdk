// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_IMPL_H_
#define MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_IMPL_H_

#include "base/time/time.h"
#include "base/threading/thread_checker.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/receiver_time_offset_estimator.h"

namespace media {
namespace cast {

// This implementation listens to three types of video events:
// 1. FRAME_ENCODED (sender side)
// 2. FRAME_ACK_SENT (receiver side)
// 3. FRAME_ACK_RECEIVED (sender side)
// There is a causal relationship between these events in that these events
// must happen in order. This class obtains the lower and upper bounds for
// the offset by taking the difference of timestamps (2) - (1) and (2) - (3),
// respectively.
// The bound will become better as the latency between the events decreases.
class ReceiverTimeOffsetEstimatorImpl : public ReceiverTimeOffsetEstimator {
 public:
  ReceiverTimeOffsetEstimatorImpl();

  virtual ~ReceiverTimeOffsetEstimatorImpl();

  // RawEventSubscriber implementations.
  virtual void OnReceiveFrameEvent(const FrameEvent& frame_event) OVERRIDE;
  virtual void OnReceivePacketEvent(const PacketEvent& packet_event) OVERRIDE;

  // ReceiverTimeOffsetEstimator implementation.
  virtual bool GetReceiverOffsetBounds(base::TimeDelta* lower_bound,
                                       base::TimeDelta* upper_bound) OVERRIDE;

 private:
  struct EventTimes {
    base::TimeTicks event_a_time;
    base::TimeTicks event_b_time;
    base::TimeTicks event_c_time;
  };

  typedef std::map<RtpTimestamp, EventTimes> EventTimesMap;

  void UpdateOffsetBounds(const EventTimes& event);

  // Fixed size storage to store event times for recent frames.
  EventTimesMap event_times_map_;

  bool bounded_;
  base::TimeDelta offset_lower_bound_;
  base::TimeDelta offset_upper_bound_;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(ReceiverTimeOffsetEstimatorImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_IMPL_H_
