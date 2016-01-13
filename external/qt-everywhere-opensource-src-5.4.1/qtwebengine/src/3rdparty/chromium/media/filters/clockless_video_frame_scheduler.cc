// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/clockless_video_frame_scheduler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/video_frame.h"

namespace media {

ClocklessVideoFrameScheduler::ClocklessVideoFrameScheduler(
    const DisplayCB& display_cb)
    : display_cb_(display_cb) {
}

ClocklessVideoFrameScheduler::~ClocklessVideoFrameScheduler() {
}

void ClocklessVideoFrameScheduler::ScheduleVideoFrame(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks /* wall_ticks */,
    const DoneCB& done_cb) {
  display_cb_.Run(frame);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(done_cb, frame, DISPLAYED));
}

void ClocklessVideoFrameScheduler::Reset() {
}

}  // namespace media
