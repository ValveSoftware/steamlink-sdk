// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// Filters a sequence of events to achieve a target frequency.
class CONTENT_EXPORT SmoothEventSampler {
 public:
  explicit SmoothEventSampler(base::TimeDelta capture_period,
                              bool events_are_reliable,
                              int redundant_capture_goal);

  // Add a new event to the event history, and return whether it ought to be
  // sampled based on the desired |capture_period|. The event is not recorded as
  // a sample until RecordSample() is called.
  bool AddEventAndConsiderSampling(base::TimeTicks event_time);

  // Operates on the last event added by AddEventAndConsiderSampling(), marking
  // it as sampled. After this point we are current in the stream of events, as
  // we have sampled the most recent event.
  void RecordSample();

  // Returns true if, at time |event_time|, sampling should occur because too
  // much time will have passed relative to the last event and/or sample.
  bool IsOverdueForSamplingAt(base::TimeTicks event_time) const;

  // Returns true if AddEventAndConsiderSampling() has been called since the
  // last call to RecordSample().
  bool HasUnrecordedEvent() const;

 private:
  const bool events_are_reliable_;
  const base::TimeDelta capture_period_;
  const int redundant_capture_goal_;
  const base::TimeDelta token_bucket_capacity_;

  base::TimeTicks current_event_;
  base::TimeTicks last_sample_;
  int overdue_sample_count_;
  base::TimeDelta token_bucket_;

  DISALLOW_COPY_AND_ASSIGN(SmoothEventSampler);
};

// VideoCaptureOracle manages the producer-side throttling of captured frames
// from a video capture device.  It is informed of every update by the device;
// this empowers it to look into the future and decide if a particular frame
// ought to be captured in order to achieve its target frame rate.
class CONTENT_EXPORT VideoCaptureOracle {
 public:
  enum Event {
    kTimerPoll,
    kCompositorUpdate,
    kSoftwarePaint,
  };

  VideoCaptureOracle(base::TimeDelta capture_period,
                     bool events_are_reliable);
  virtual ~VideoCaptureOracle() {}

  // Record an event of type |event|, and decide whether the caller should do a
  // frame capture immediately. Decisions of the oracle are final: the caller
  // must do what it is told.
  bool ObserveEventAndDecideCapture(Event event, base::TimeTicks event_time);

  // Record the start of a capture.  Returns a frame_number to be used with
  // CompleteCapture().
  int RecordCapture();

  // Record the completion of a capture.  Returns true iff the captured frame
  // should be delivered.
  bool CompleteCapture(int frame_number, base::TimeTicks timestamp);

  base::TimeDelta capture_period() const { return capture_period_; }

 private:

  // Time between frames.
  const base::TimeDelta capture_period_;

  // Incremented every time a paint or update event occurs.
  int frame_number_;

  // Stores the frame number from the last delivered frame.
  int last_delivered_frame_number_;

  // Stores the timestamp of the last delivered frame.
  base::TimeTicks last_delivered_frame_timestamp_;

  // Tracks present/paint history.
  SmoothEventSampler sampler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_
