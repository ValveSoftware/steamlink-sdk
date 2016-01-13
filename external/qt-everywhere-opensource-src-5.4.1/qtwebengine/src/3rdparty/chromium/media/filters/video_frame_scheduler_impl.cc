// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_frame_scheduler_impl.h"

#include <list>

#include "base/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "media/base/video_frame.h"

namespace media {

VideoFrameSchedulerImpl::VideoFrameSchedulerImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const DisplayCB& display_cb)
    : task_runner_(task_runner),
      display_cb_(display_cb),
      tick_clock_(new base::DefaultTickClock()) {
}

VideoFrameSchedulerImpl::~VideoFrameSchedulerImpl() {
}

void VideoFrameSchedulerImpl::ScheduleVideoFrame(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks wall_ticks,
    const DoneCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!frame->end_of_stream());
  pending_frames_.push(PendingFrame(frame, wall_ticks, done_cb));
  ResetTimerIfNecessary();
}

void VideoFrameSchedulerImpl::Reset() {
  pending_frames_ = PendingFrameQueue();
  timer_.Stop();
}

void VideoFrameSchedulerImpl::SetTickClockForTesting(
    scoped_ptr<base::TickClock> tick_clock) {
  tick_clock_.swap(tick_clock);
}

void VideoFrameSchedulerImpl::ResetTimerIfNecessary() {
  if (pending_frames_.empty()) {
    DCHECK(!timer_.IsRunning());
    return;
  }

  // Negative times will schedule the callback to run immediately.
  timer_.Stop();
  timer_.Start(FROM_HERE,
               pending_frames_.top().wall_ticks - tick_clock_->NowTicks(),
               base::Bind(&VideoFrameSchedulerImpl::OnTimerFired,
                          base::Unretained(this)));
}

void VideoFrameSchedulerImpl::OnTimerFired() {
  base::TimeTicks now = tick_clock_->NowTicks();

  // Move all frames that have reached their deadline into a separate queue.
  std::list<PendingFrame> expired_frames;
  while (!pending_frames_.empty() && pending_frames_.top().wall_ticks <= now) {
    expired_frames.push_back(pending_frames_.top());
    pending_frames_.pop();
  }

  // Signal that all frames except for the last one as dropped.
  while (expired_frames.size() > 1) {
    expired_frames.front().done_cb.Run(expired_frames.front().frame, DROPPED);
    expired_frames.pop_front();
  }

  // Display the last expired frame.
  if (!expired_frames.empty()) {
    display_cb_.Run(expired_frames.front().frame);
    expired_frames.front().done_cb.Run(expired_frames.front().frame, DISPLAYED);
    expired_frames.pop_front();
  }

  ResetTimerIfNecessary();
}

VideoFrameSchedulerImpl::PendingFrame::PendingFrame(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks wall_ticks,
    const DoneCB& done_cb)
    : frame(frame), wall_ticks(wall_ticks), done_cb(done_cb) {
}

VideoFrameSchedulerImpl::PendingFrame::~PendingFrame() {
}

bool VideoFrameSchedulerImpl::PendingFrame::operator<(
    const PendingFrame& other) const {
  // Flip the comparison as std::priority_queue<T>::top() returns the largest
  // element.
  //
  // Assume video frames with identical timestamps contain identical content.
  return wall_ticks > other.wall_ticks;
}

}  // namespace media
