// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_frame_scheduler_proxy.h"

#include "base/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"

namespace media {

VideoFrameSchedulerProxy::VideoFrameSchedulerProxy(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& scheduler_runner,
    scoped_ptr<VideoFrameScheduler> scheduler)
    : task_runner_(task_runner),
      scheduler_runner_(scheduler_runner),
      scheduler_(scheduler.Pass()),
      weak_factory_(this) {
}

VideoFrameSchedulerProxy::~VideoFrameSchedulerProxy() {
  scheduler_runner_->DeleteSoon(FROM_HERE, scheduler_.release());
}

void VideoFrameSchedulerProxy::ScheduleVideoFrame(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks wall_ticks,
    const DoneCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  scheduler_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameScheduler::ScheduleVideoFrame,
                 base::Unretained(scheduler_.get()),
                 frame,
                 wall_ticks,
                 BindToCurrentLoop(done_cb)));
}

void VideoFrameSchedulerProxy::Reset() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  scheduler_runner_->PostTask(FROM_HERE,
                              base::Bind(&VideoFrameScheduler::Reset,
                                         base::Unretained(scheduler_.get())));
}

}  // namespace media
