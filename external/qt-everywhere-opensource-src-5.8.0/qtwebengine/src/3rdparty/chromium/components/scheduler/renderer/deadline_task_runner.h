// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_DEADLINE_TASK_RUNNER_H_
#define COMPONENTS_SCHEDULER_RENDERER_DEADLINE_TASK_RUNNER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/scheduler/base/cancelable_closure_holder.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

// Runs a posted task at latest by a given deadline, but possibly sooner.
class SCHEDULER_EXPORT DeadlineTaskRunner {
 public:
  DeadlineTaskRunner(const base::Closure& callback,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~DeadlineTaskRunner();

  // If there is no outstanding task then a task is posted to run after |delay|.
  // If there is an outstanding task which is scheduled to run:
  //   a) sooner - then this is a NOP.
  //   b) later - then the outstanding task is cancelled and a new task is
  //              posted to run after |delay|.
  //
  // Once the deadline task has run, we reset.
  void SetDeadline(const tracked_objects::Location& from_here,
                   base::TimeDelta delay,
                   base::TimeTicks now);

 private:
  void RunInternal();

  CancelableClosureHolder cancelable_run_internal_;
  base::Closure callback_;
  base::TimeTicks deadline_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DeadlineTaskRunner);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_DEADLINE_TASK_RUNNER_H_
