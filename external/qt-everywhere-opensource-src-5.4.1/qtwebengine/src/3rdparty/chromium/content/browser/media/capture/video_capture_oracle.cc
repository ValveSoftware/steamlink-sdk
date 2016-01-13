// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/video_capture_oracle.h"

#include "base/debug/trace_event.h"

namespace content {

namespace {

// This value controls how many redundant, timer-base captures occur when the
// content is static. Redundantly capturing the same frame allows iterative
// quality enhancement, and also allows the buffer to fill in "buffered mode".
//
// TODO(nick): Controlling this here is a hack and a layering violation, since
// it's a strategy specific to the WebRTC consumer, and probably just papers
// over some frame dropping and quality bugs. It should either be controlled at
// a higher level, or else redundant frame generation should be pushed down
// further into the WebRTC encoding stack.
const int kNumRedundantCapturesOfStaticContent = 200;

}  // anonymous namespace

VideoCaptureOracle::VideoCaptureOracle(base::TimeDelta capture_period,
                                       bool events_are_reliable)
    : capture_period_(capture_period),
      frame_number_(0),
      last_delivered_frame_number_(0),
      sampler_(capture_period_,
               events_are_reliable,
               kNumRedundantCapturesOfStaticContent) {}

bool VideoCaptureOracle::ObserveEventAndDecideCapture(
    Event event,
    base::TimeTicks event_time) {
  // Record |event| and decide whether it's a good time to capture.
  const bool content_is_dirty = (event == kCompositorUpdate ||
                                 event == kSoftwarePaint);
  bool should_sample;
  if (content_is_dirty) {
    frame_number_++;
    should_sample = sampler_.AddEventAndConsiderSampling(event_time);
  } else {
    should_sample = sampler_.IsOverdueForSamplingAt(event_time);
  }
  return should_sample;
}

int VideoCaptureOracle::RecordCapture() {
  sampler_.RecordSample();
  return frame_number_;
}

bool VideoCaptureOracle::CompleteCapture(int frame_number,
                                         base::TimeTicks timestamp) {
  // Drop frame if previous frame number is higher or we're trying to deliver
  // a frame with the same timestamp.
  if (last_delivered_frame_number_ > frame_number ||
      last_delivered_frame_timestamp_ == timestamp) {
    LOG(ERROR) << "Frame with same timestamp or out of order delivery. "
               << "Dropping frame.";
    return false;
  }

  if (last_delivered_frame_timestamp_ > timestamp) {
    // We should not get here unless time was adjusted backwards.
    LOG(ERROR) << "Frame with past timestamp (" << timestamp.ToInternalValue()
               << ") was delivered";
  }

  last_delivered_frame_number_ = frame_number;
  last_delivered_frame_timestamp_ = timestamp;

  return true;
}

SmoothEventSampler::SmoothEventSampler(base::TimeDelta capture_period,
                                       bool events_are_reliable,
                                       int redundant_capture_goal)
    :  events_are_reliable_(events_are_reliable),
       capture_period_(capture_period),
       redundant_capture_goal_(redundant_capture_goal),
       token_bucket_capacity_(capture_period + capture_period / 2),
       overdue_sample_count_(0),
       token_bucket_(token_bucket_capacity_) {
  DCHECK_GT(capture_period_.InMicroseconds(), 0);
}

bool SmoothEventSampler::AddEventAndConsiderSampling(
    base::TimeTicks event_time) {
  DCHECK(!event_time.is_null());

  // Add tokens to the bucket based on advancement in time.  Then, re-bound the
  // number of tokens in the bucket.  Overflow occurs when there is too much
  // time between events (a common case), or when RecordSample() is not being
  // called often enough (a bug).  On the other hand, if RecordSample() is being
  // called too often (e.g., as a reaction to IsOverdueForSamplingAt()), the
  // bucket will underflow.
  if (!current_event_.is_null()) {
    if (current_event_ < event_time) {
      token_bucket_ += event_time - current_event_;
      if (token_bucket_ > token_bucket_capacity_)
        token_bucket_ = token_bucket_capacity_;
    }
    // Side note: If the system clock is reset, causing |current_event_| to be
    // greater than |event_time|, everything here will simply gracefully adjust.
    if (token_bucket_ < base::TimeDelta())
      token_bucket_ = base::TimeDelta();
    TRACE_COUNTER1("mirroring",
                   "MirroringTokenBucketUsec",
                   std::max<int64>(0, token_bucket_.InMicroseconds()));
  }
  current_event_ = event_time;

  // Return true if one capture period's worth of tokens are in the bucket.
  return token_bucket_ >= capture_period_;
}

void SmoothEventSampler::RecordSample() {
  token_bucket_ -= capture_period_;
  TRACE_COUNTER1("mirroring",
                 "MirroringTokenBucketUsec",
                 std::max<int64>(0, token_bucket_.InMicroseconds()));

  bool was_paused = overdue_sample_count_ == redundant_capture_goal_;
  if (HasUnrecordedEvent()) {
    last_sample_ = current_event_;
    overdue_sample_count_ = 0;
  } else {
    ++overdue_sample_count_;
  }
  bool is_paused = overdue_sample_count_ == redundant_capture_goal_;

  VLOG_IF(0, !was_paused && is_paused)
      << "Tab content unchanged for " << redundant_capture_goal_
      << " frames; capture will halt until content changes.";
  VLOG_IF(0, was_paused && !is_paused)
      << "Content changed; capture will resume.";
}

bool SmoothEventSampler::IsOverdueForSamplingAt(base::TimeTicks event_time)
    const {
  DCHECK(!event_time.is_null());

  // If we don't get events on compositor updates on this platform, then we
  // don't reliably know whether we're dirty.
  if (events_are_reliable_) {
    if (!HasUnrecordedEvent() &&
        overdue_sample_count_ >= redundant_capture_goal_) {
      return false;  // Not dirty.
    }
  }

  if (last_sample_.is_null())
    return true;

  // If we're dirty but not yet old, then we've recently gotten updates, so we
  // won't request a sample just yet.
  base::TimeDelta dirty_interval = event_time - last_sample_;
  if (dirty_interval < capture_period_ * 4)
    return false;
  else
    return true;
}

bool SmoothEventSampler::HasUnrecordedEvent() const {
  return !current_event_.is_null() && current_event_ != last_sample_;
}

}  // namespace content
