// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/logging.h"
#include "blimp/engine/renderer/frame_scheduler.h"

namespace blimp {
namespace engine {
namespace {
// This is the temporary frame delay to keep pages which make animation requests
// but don't mutate the state on the engine from running main frames
// back-to-back. We need smarter throttling of engine updates. See
// crbug.com/597829.
constexpr base::TimeDelta kDefaultFrameDelay =
    base::TimeDelta::FromMilliseconds(30);
}  // namespace

FrameScheduler::FrameScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    FrameSchedulerClient* client)
    : next_frame_time_(base::TimeTicks::Now()),
      frame_delay_(kDefaultFrameDelay),
      client_(client) {
  DCHECK(client_);
  frame_tick_timer_.SetTaskRunner(std::move(task_runner));
}

FrameScheduler::~FrameScheduler() = default;

void FrameScheduler::ScheduleFrameUpdate() {
  if (needs_frame_update_)
    return;

  needs_frame_update_ = true;
  ScheduleFrameUpdateIfNecessary();
}

void FrameScheduler::DidSendFrameUpdateToClient() {
  DCHECK(in_frame_update_);
  DCHECK(!frame_ack_pending_) << "We can have only frame update in flight";

  frame_ack_pending_ = true;
}

void FrameScheduler::DidReceiveFrameUpdateAck() {
  DCHECK(frame_ack_pending_);

  frame_ack_pending_ = false;
  ScheduleFrameUpdateIfNecessary();
}

void FrameScheduler::ScheduleFrameUpdateIfNecessary() {
  // If we can't produce main frame updates right now, don't schedule a task.
  if (!ShouldProduceFrameUpdates()) {
    return;
  }

  // If a task has already been scheduled, we don't need to schedule again.
  if (frame_tick_timer_.IsRunning())
    return;

  base::TimeDelta delay = next_frame_time_ - base::TimeTicks::Now();
  frame_tick_timer_.Start(
      FROM_HERE, delay,
      base::Bind(&FrameScheduler::StartFrameUpdate, base::Unretained(this)));
}

bool FrameScheduler::ShouldProduceFrameUpdates() const {
  return needs_frame_update_ && !frame_ack_pending_;
}

void FrameScheduler::StartFrameUpdate() {
  DCHECK(!in_frame_update_);
  DCHECK(needs_frame_update_);

  // If an update was sent to the client since this request, we can not start
  // another frame. Early out here, we'll come back when an Ack is received from
  // the client.
  if (frame_ack_pending_)
    return;

  needs_frame_update_ = false;
  next_frame_time_ = base::TimeTicks::Now() + frame_delay_;
  {
    base::AutoReset<bool> in_frame_update(&in_frame_update_, true);
    client_->StartFrameUpdate();
  }
}

}  // namespace engine
}  // namespace blimp
