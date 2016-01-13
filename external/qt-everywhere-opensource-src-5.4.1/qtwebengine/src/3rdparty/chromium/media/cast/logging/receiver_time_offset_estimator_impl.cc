// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "media/cast/logging/receiver_time_offset_estimator_impl.h"

namespace media {
namespace cast {

// This should be large enough so that we can collect all 3 events before
// the entry gets removed from the map.
const size_t kMaxEventTimesMapSize = 100;

ReceiverTimeOffsetEstimatorImpl::ReceiverTimeOffsetEstimatorImpl()
    : bounded_(false) {}

ReceiverTimeOffsetEstimatorImpl::~ReceiverTimeOffsetEstimatorImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ReceiverTimeOffsetEstimatorImpl::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (frame_event.media_type != VIDEO_EVENT)
    return;

  CastLoggingEvent event = frame_event.type;
  if (event != FRAME_ENCODED && event != FRAME_ACK_SENT &&
      event != FRAME_ACK_RECEIVED)
    return;

  EventTimesMap::iterator it = event_times_map_.find(frame_event.rtp_timestamp);
  if (it == event_times_map_.end()) {
    EventTimes event_times;
    it = event_times_map_.insert(std::make_pair(frame_event.rtp_timestamp,
                                                event_times)).first;
  }
  switch (event) {
    case FRAME_ENCODED:
      // Encode is supposed to happen only once. If we see duplicate event,
      // throw away the entry.
      if (it->second.event_a_time.is_null()) {
        it->second.event_a_time = frame_event.timestamp;
      } else {
        event_times_map_.erase(it);
        return;
      }
      break;
    case FRAME_ACK_SENT:
      if (it->second.event_b_time.is_null()) {
        it->second.event_b_time = frame_event.timestamp;
      } else if (it->second.event_b_time != frame_event.timestamp) {
        // Duplicate ack sent events are normal due to RTCP redundancy,
        // but they must have the same event timestamp.
        event_times_map_.erase(it);
        return;
      }
      break;
    case FRAME_ACK_RECEIVED:
      // If there are duplicate ack received events, pick the one with the
      // smallest event timestamp so we can get a better bound.
      if (it->second.event_c_time.is_null()) {
        it->second.event_c_time = frame_event.timestamp;
      } else {
        it->second.event_c_time =
            std::min(frame_event.timestamp, it->second.event_c_time);
      }
      break;
    default:
      NOTREACHED();
  }

  if (!it->second.event_a_time.is_null() &&
      !it->second.event_b_time.is_null() &&
      !it->second.event_c_time.is_null()) {
    UpdateOffsetBounds(it->second);
    event_times_map_.erase(it);
  }

  // Keep the map size at most |kMaxEventTimesMapSize|.
  if (event_times_map_.size() > kMaxEventTimesMapSize)
    event_times_map_.erase(event_times_map_.begin());
}

bool ReceiverTimeOffsetEstimatorImpl::GetReceiverOffsetBounds(
    base::TimeDelta* lower_bound,
    base::TimeDelta* upper_bound) {
  if (!bounded_)
    return false;

  *lower_bound = offset_lower_bound_;
  *upper_bound = offset_upper_bound_;
  return true;
}

void ReceiverTimeOffsetEstimatorImpl::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  // Not interested in packet events.
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ReceiverTimeOffsetEstimatorImpl::UpdateOffsetBounds(
    const EventTimes& event) {
  base::TimeDelta lower_bound = event.event_b_time - event.event_c_time;
  base::TimeDelta upper_bound = event.event_b_time - event.event_a_time;

  if (bounded_) {
    lower_bound = std::max(lower_bound, offset_lower_bound_);
    upper_bound = std::min(upper_bound, offset_upper_bound_);
  }

  if (lower_bound > upper_bound) {
    VLOG(2) << "Got bogus offset bound values [" << lower_bound.InMilliseconds()
            << ", " << upper_bound.InMilliseconds() << "].";
    return;
  }

  offset_lower_bound_ = lower_bound;
  offset_upper_bound_ = upper_bound;
  bounded_ = true;
}

}  // namespace cast
}  // namespace media
