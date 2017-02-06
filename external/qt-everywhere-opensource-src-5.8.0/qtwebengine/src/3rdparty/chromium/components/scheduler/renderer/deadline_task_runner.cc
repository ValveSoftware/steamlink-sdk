// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/deadline_task_runner.h"

#include "base/bind.h"

namespace scheduler {

DeadlineTaskRunner::DeadlineTaskRunner(
    const base::Closure& callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : callback_(callback), task_runner_(task_runner) {
  cancelable_run_internal_.Reset(
      base::Bind(&DeadlineTaskRunner::RunInternal, base::Unretained(this)));
}

DeadlineTaskRunner::~DeadlineTaskRunner() {
}

void DeadlineTaskRunner::SetDeadline(const tracked_objects::Location& from_here,
                                     base::TimeDelta delay,
                                     base::TimeTicks now) {
  DCHECK(delay > base::TimeDelta());
  base::TimeTicks deadline = now + delay;
  if (deadline_.is_null() || deadline < deadline_) {
    deadline_ = deadline;
    cancelable_run_internal_.Cancel();
    task_runner_->PostDelayedTask(from_here,
                                  cancelable_run_internal_.callback(), delay);
  }
}

void DeadlineTaskRunner::RunInternal() {
  deadline_ = base::TimeTicks();
  callback_.Run();
}

}  // namespace scheduler
