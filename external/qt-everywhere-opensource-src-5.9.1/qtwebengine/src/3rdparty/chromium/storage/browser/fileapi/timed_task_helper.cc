// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/timed_task_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"

namespace storage {

struct TimedTaskHelper::Tracker {
  explicit Tracker(TimedTaskHelper* timer) : timer(timer) {}

  ~Tracker() {
    if (timer)
      timer->tracker_ = NULL;
  }

  TimedTaskHelper* timer;
};

TimedTaskHelper::TimedTaskHelper(base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner),
      tracker_(NULL) {
}

TimedTaskHelper::~TimedTaskHelper() {
  if (tracker_)
    tracker_->timer = NULL;
}

bool TimedTaskHelper::IsRunning() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return tracker_ != NULL;
}

void TimedTaskHelper::Start(
    const tracked_objects::Location& posted_from,
    base::TimeDelta delay,
    const base::Closure& user_task) {
  posted_from_ = posted_from;
  delay_ = delay;
  user_task_ = user_task;
  Reset();
}

void TimedTaskHelper::Reset() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!user_task_.is_null());
  desired_run_time_ = base::TimeTicks::Now() + delay_;

  if (tracker_)
    return;

  // Initialize the tracker for the first time.
  tracker_ = new Tracker(this);
  PostDelayedTask(base::WrapUnique(tracker_), delay_);
}

// static
void TimedTaskHelper::Fired(std::unique_ptr<Tracker> tracker) {
  if (!tracker->timer)
    return;
  TimedTaskHelper* timer = tracker->timer;
  timer->OnFired(std::move(tracker));
}

void TimedTaskHelper::OnFired(std::unique_ptr<Tracker> tracker) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::TimeTicks now = base::TimeTicks::Now();
  if (desired_run_time_ > now) {
    PostDelayedTask(std::move(tracker), desired_run_time_ - now);
    return;
  }
  tracker.reset();
  base::Closure task = user_task_;
  user_task_.Reset();
  task.Run();
}

void TimedTaskHelper::PostDelayedTask(std::unique_ptr<Tracker> tracker,
                                      base::TimeDelta delay) {
  task_runner_->PostDelayedTask(
      posted_from_,
      base::Bind(&TimedTaskHelper::Fired, base::Passed(&tracker)),
      delay);
}

}  // namespace storage
