// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/test_video_frame_scheduler.h"

#include "media/base/video_frame.h"

namespace media {

TestVideoFrameScheduler::ScheduledFrame::ScheduledFrame(
    const scoped_refptr<VideoFrame> frame,
    base::TimeTicks wall_ticks,
    const DoneCB& done_cb)
    : frame(frame), wall_ticks(wall_ticks), done_cb(done_cb) {
}

TestVideoFrameScheduler::ScheduledFrame::~ScheduledFrame() {
}

TestVideoFrameScheduler::TestVideoFrameScheduler() {
}

TestVideoFrameScheduler::~TestVideoFrameScheduler() {
}

void TestVideoFrameScheduler::ScheduleVideoFrame(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks wall_ticks,
    const DoneCB& done_cb) {
  scheduled_frames_.push_back(ScheduledFrame(frame, wall_ticks, done_cb));
}

void TestVideoFrameScheduler::Reset() {
  scheduled_frames_.clear();
}

void TestVideoFrameScheduler::DisplayFramesUpTo(base::TimeTicks wall_ticks) {
  RunDoneCBForFramesUpTo(wall_ticks, DISPLAYED);
}

void TestVideoFrameScheduler::DropFramesUpTo(base::TimeTicks wall_ticks) {
  RunDoneCBForFramesUpTo(wall_ticks, DROPPED);
}

void TestVideoFrameScheduler::RunDoneCBForFramesUpTo(base::TimeTicks wall_ticks,
                                                     Reason reason) {
  std::vector<ScheduledFrame> done_frames;
  std::vector<ScheduledFrame> remaining_frames;

  for (size_t i = 0; i < scheduled_frames_.size(); ++i) {
    if (scheduled_frames_[i].wall_ticks <= wall_ticks) {
      done_frames.push_back(scheduled_frames_[i]);
    } else {
      remaining_frames.push_back(scheduled_frames_[i]);
    }
  }

  scheduled_frames_.swap(remaining_frames);

  for (size_t i = 0; i < done_frames.size(); ++i) {
    done_frames[i].done_cb.Run(done_frames[i].frame, reason);
  }
}

}  // namespace media
