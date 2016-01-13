// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_PROXY_H_
#define MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "media/filters/video_frame_scheduler.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Provides a thread-safe proxy for a VideoFrameScheduler. Typical use is to
// use a real VideoFrameScheduler on the task runner responsible for graphics
// display and provide a proxy on the task runner responsible for background
// video decoding.
class MEDIA_EXPORT VideoFrameSchedulerProxy : public VideoFrameScheduler {
 public:
  // |task_runner| is the runner that this object will be called on.
  // |scheduler_runner| is the runner that |scheduler| will be called on.
  // |scheduler| will be deleted on |scheduler_runner|.
  VideoFrameSchedulerProxy(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& scheduler_runner,
      scoped_ptr<VideoFrameScheduler> scheduler);
  virtual ~VideoFrameSchedulerProxy();

  // VideoFrameScheduler implementation.
  virtual void ScheduleVideoFrame(const scoped_refptr<VideoFrame>& frame,
                                  base::TimeTicks wall_ticks,
                                  const DoneCB& done_cb) OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> scheduler_runner_;
  scoped_ptr<VideoFrameScheduler> scheduler_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoFrameSchedulerProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameSchedulerProxy);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_PROXY_H_
