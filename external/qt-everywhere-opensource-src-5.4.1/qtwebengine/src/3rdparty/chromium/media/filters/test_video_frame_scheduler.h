// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_TEST_VIDEO_FRAME_SCHEDULER_H_
#define MEDIA_FILTERS_TEST_VIDEO_FRAME_SCHEDULER_H_

#include <vector>

#include "media/filters/video_frame_scheduler.h"

namespace media {

// A scheduler that queues frames until told otherwise.
class TestVideoFrameScheduler : public VideoFrameScheduler {
 public:
  struct ScheduledFrame {
    ScheduledFrame(const scoped_refptr<VideoFrame> frame,
                   base::TimeTicks wall_ticks,
                   const DoneCB& done_cb);
    ~ScheduledFrame();

    scoped_refptr<VideoFrame> frame;
    base::TimeTicks wall_ticks;
    DoneCB done_cb;
  };

  TestVideoFrameScheduler();
  virtual ~TestVideoFrameScheduler();

  // VideoFrameScheduler implementation.
  virtual void ScheduleVideoFrame(const scoped_refptr<VideoFrame>& frame,
                                  base::TimeTicks wall_ticks,
                                  const DoneCB& done_cb) OVERRIDE;
  virtual void Reset() OVERRIDE;

  // Displays all frames with scheduled times <= |wall_ticks|.
  void DisplayFramesUpTo(base::TimeTicks wall_ticks);

  // Drops all frames with scheduled times <= |wall_ticks|.
  void DropFramesUpTo(base::TimeTicks wall_ticks);

  const std::vector<ScheduledFrame>& scheduled_frames() const {
    return scheduled_frames_;
  }

 private:
  void RunDoneCBForFramesUpTo(base::TimeTicks wall_ticks, Reason reason);

  std::vector<ScheduledFrame> scheduled_frames_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoFrameScheduler);
};

}  // namespace media

#endif  // MEDIA_FILTERS_TEST_VIDEO_FRAME_SCHEDULER_H_
