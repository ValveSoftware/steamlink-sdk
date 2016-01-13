// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_IMPL_H_
#define MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_IMPL_H_

#include <queue>

#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "media/filters/video_frame_scheduler.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}

namespace media {

// A scheduler that uses delayed tasks on a task runner for timing the display
// of video frames.
//
// Single threaded. Calls must be on |task_runner|.
class MEDIA_EXPORT VideoFrameSchedulerImpl : public VideoFrameScheduler {
 public:
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&)> DisplayCB;

  // |task_runner| is used for scheduling the delayed tasks.
  // |display_cb| is run when a frame is to be displayed.
  VideoFrameSchedulerImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const DisplayCB& display_cb);
  virtual ~VideoFrameSchedulerImpl();

  // VideoFrameScheduler implementation.
  virtual void ScheduleVideoFrame(const scoped_refptr<VideoFrame>& frame,
                                  base::TimeTicks wall_ticks,
                                  const DoneCB& done_cb) OVERRIDE;
  virtual void Reset() OVERRIDE;

  void SetTickClockForTesting(scoped_ptr<base::TickClock> tick_clock);

 private:
  void ResetTimerIfNecessary();
  void OnTimerFired();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DisplayCB display_cb_;
  scoped_ptr<base::TickClock> tick_clock_;
  base::OneShotTimer<VideoFrameScheduler> timer_;

  struct PendingFrame {
    PendingFrame(const scoped_refptr<VideoFrame>& frame,
                 base::TimeTicks wall_ticks,
                 const DoneCB& done_cb);
    ~PendingFrame();

    // For use with std::priority_queue<T>.
    bool operator<(const PendingFrame& other) const;

    scoped_refptr<VideoFrame> frame;
    base::TimeTicks wall_ticks;
    DoneCB done_cb;
  };
  typedef std::priority_queue<PendingFrame> PendingFrameQueue;
  PendingFrameQueue pending_frames_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameSchedulerImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_IMPL_H_
