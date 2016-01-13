// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CLOCKLESS_VIDEO_FRAME_SCHEDULER_H_
#define MEDIA_FILTERS_CLOCKLESS_VIDEO_FRAME_SCHEDULER_H_

#include "media/filters/video_frame_scheduler.h"

namespace media {

// A scheduler that immediately displays frames.
class ClocklessVideoFrameScheduler : public VideoFrameScheduler {
 public:
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&)> DisplayCB;

  explicit ClocklessVideoFrameScheduler(const DisplayCB& display_cb);
  virtual ~ClocklessVideoFrameScheduler();

  // VideoFrameScheduler implementation.
  virtual void ScheduleVideoFrame(const scoped_refptr<VideoFrame>& frame,
                                  base::TimeTicks wall_ticks,
                                  const DoneCB& done_cb) OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  DisplayCB display_cb_;

  DISALLOW_COPY_AND_ASSIGN(ClocklessVideoFrameScheduler);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CLOCKLESS_VIDEO_FRAME_SCHEDULER_H_
