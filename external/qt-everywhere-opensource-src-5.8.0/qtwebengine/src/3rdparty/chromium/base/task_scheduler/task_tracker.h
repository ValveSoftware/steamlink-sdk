// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_TASK_TRACKER_H_
#define BASE_TASK_SCHEDULER_TASK_TRACKER_H_

#include <memory>

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/synchronization/condition_variable.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_traits.h"

namespace base {
namespace internal {

// All tasks go through the scheduler's TaskTracker when they are posted and
// when they are executed. The TaskTracker enforces shutdown semantics and takes
// care of tracing and profiling. This class is thread-safe.
class BASE_EXPORT TaskTracker {
 public:
  TaskTracker();
  ~TaskTracker();

  // Synchronously shuts down the scheduler. Once this is called, only tasks
  // posted with the BLOCK_SHUTDOWN behavior will be run. Returns when:
  // - All SKIP_ON_SHUTDOWN tasks that were already running have completed their
  //   execution.
  // - All posted BLOCK_SHUTDOWN tasks have completed their execution.
  // CONTINUE_ON_SHUTDOWN tasks still may be running after Shutdown returns.
  // This can only be called once.
  void Shutdown();

  // Informs this TaskTracker that |task| is about to be posted. Returns true if
  // this operation is allowed (|task| should be posted if-and-only-if it is).
  bool WillPostTask(const Task* task);

  // Runs |task| unless the current shutdown state prevents that. WillPostTask()
  // must have allowed |task| to be posted.
  void RunTask(const Task* task);

  // Returns true while shutdown is in progress (i.e. Shutdown() has been called
  // but hasn't returned).
  bool IsShuttingDownForTesting() const;

  bool shutdown_completed() const {
    AutoSchedulerLock auto_lock(lock_);
    return shutdown_completed_;
  }

 private:
  // Called before WillPostTask() informs the tracing system that a task has
  // been posted. Updates |num_tasks_blocking_shutdown_| if necessary and
  // returns true if the current shutdown state allows the task to be posted.
  bool BeforePostTask(TaskShutdownBehavior shutdown_behavior);

  // Called before a task with |shutdown_behavior| is run by RunTask(). Updates
  // |num_tasks_blocking_shutdown_| if necessary and returns true if the current
  // shutdown state allows the task to be run.
  bool BeforeRunTask(TaskShutdownBehavior shutdown_behavior);

  // Called after a task with |shutdown_behavior| has been run by RunTask().
  // Updates |num_tasks_blocking_shutdown_| and signals |shutdown_cv_| if
  // necessary.
  void AfterRunTask(TaskShutdownBehavior shutdown_behavior);

  // Synchronizes access to all members.
  mutable SchedulerLock lock_;

  // Condition variable signaled when |num_tasks_blocking_shutdown_| reaches
  // zero while shutdown is in progress. Null if shutdown isn't in progress.
  std::unique_ptr<ConditionVariable> shutdown_cv_;

  // Number of tasks blocking shutdown.
  size_t num_tasks_blocking_shutdown_ = 0;

  // Number of BLOCK_SHUTDOWN tasks posted during shutdown.
  HistogramBase::Sample num_block_shutdown_tasks_posted_during_shutdown_ = 0;

  // True once Shutdown() has returned. No new task can be scheduled after this.
  bool shutdown_completed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TaskTracker);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_TASK_TRACKER_H_
