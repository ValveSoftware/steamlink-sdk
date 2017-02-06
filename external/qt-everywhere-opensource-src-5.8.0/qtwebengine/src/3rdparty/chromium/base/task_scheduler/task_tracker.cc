// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/task_tracker.h"

#include "base/callback.h"
#include "base/debug/task_annotator.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace internal {

namespace {

const char kQueueFunctionName[] = "base::PostTask";

// This name conveys that a Task is run by the task scheduler without revealing
// its implementation details.
const char kRunFunctionName[] = "TaskSchedulerRunTask";

// Upper bound for the
// TaskScheduler.BlockShutdownTasksPostedDuringShutdown histogram.
const HistogramBase::Sample kMaxBlockShutdownTasksPostedDuringShutdown = 1000;

void RecordNumBlockShutdownTasksPostedDuringShutdown(
    HistogramBase::Sample value) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "TaskScheduler.BlockShutdownTasksPostedDuringShutdown", value, 1,
      kMaxBlockShutdownTasksPostedDuringShutdown, 50);
}

}  // namespace

TaskTracker::TaskTracker() = default;
TaskTracker::~TaskTracker() = default;

void TaskTracker::Shutdown() {
  AutoSchedulerLock auto_lock(lock_);

  // This method should only be called once.
  DCHECK(!shutdown_completed_);
  DCHECK(!shutdown_cv_);

  shutdown_cv_ = lock_.CreateConditionVariable();

  // Wait until the number of tasks blocking shutdown is zero.
  while (num_tasks_blocking_shutdown_ != 0)
    shutdown_cv_->Wait();

  shutdown_cv_.reset();
  shutdown_completed_ = true;

  // Record the TaskScheduler.BlockShutdownTasksPostedDuringShutdown if less
  // than |kMaxBlockShutdownTasksPostedDuringShutdown| BLOCK_SHUTDOWN tasks were
  // posted during shutdown. Otherwise, the histogram has already been recorded
  // in BeforePostTask().
  if (num_block_shutdown_tasks_posted_during_shutdown_ <
      kMaxBlockShutdownTasksPostedDuringShutdown) {
    RecordNumBlockShutdownTasksPostedDuringShutdown(
        num_block_shutdown_tasks_posted_during_shutdown_);
  }
}

bool TaskTracker::WillPostTask(const Task* task) {
  DCHECK(task);

  if (!BeforePostTask(task->traits.shutdown_behavior()))
    return false;

  debug::TaskAnnotator task_annotator;
  task_annotator.DidQueueTask(kQueueFunctionName, *task);

  return true;
}

void TaskTracker::RunTask(const Task* task) {
  DCHECK(task);

  const TaskShutdownBehavior shutdown_behavior =
      task->traits.shutdown_behavior();
  if (!BeforeRunTask(shutdown_behavior))
    return;

  // All tasks run through here and the scheduler itself doesn't use singletons.
  // Therefore, it isn't necessary to reset the singleton allowed bit after
  // running the task.
  ThreadRestrictions::SetSingletonAllowed(
      task->traits.shutdown_behavior() !=
      TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN);

  {
    // Set up TaskRunnerHandle as expected for the scope of the task.
    std::unique_ptr<SequencedTaskRunnerHandle> sequenced_task_runner_handle;
    std::unique_ptr<ThreadTaskRunnerHandle> single_thread_task_runner_handle;
    DCHECK(!task->sequenced_task_runner_ref ||
           !task->single_thread_task_runner_ref);
    if (task->sequenced_task_runner_ref) {
      sequenced_task_runner_handle.reset(
          new SequencedTaskRunnerHandle(task->sequenced_task_runner_ref));
    } else if (task->single_thread_task_runner_ref) {
      single_thread_task_runner_handle.reset(
          new ThreadTaskRunnerHandle(task->single_thread_task_runner_ref));
    }

    TRACE_TASK_EXECUTION(kRunFunctionName, *task);

    debug::TaskAnnotator task_annotator;
    task_annotator.RunTask(kQueueFunctionName, *task);
  }

  AfterRunTask(shutdown_behavior);
}

bool TaskTracker::IsShuttingDownForTesting() const {
  AutoSchedulerLock auto_lock(lock_);
  return !!shutdown_cv_;
}

bool TaskTracker::BeforePostTask(TaskShutdownBehavior shutdown_behavior) {
  AutoSchedulerLock auto_lock(lock_);

  if (shutdown_completed_) {
    // A BLOCK_SHUTDOWN task posted after shutdown has completed is an ordering
    // bug. This DCHECK aims to catch those early.
    DCHECK_NE(shutdown_behavior, TaskShutdownBehavior::BLOCK_SHUTDOWN);

    // No task is allowed to be posted after shutdown.
    return false;
  }

  if (shutdown_behavior == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // BLOCK_SHUTDOWN tasks block shutdown between the moment they are posted
    // and the moment they complete their execution.
    ++num_tasks_blocking_shutdown_;

    if (shutdown_cv_) {
      ++num_block_shutdown_tasks_posted_during_shutdown_;

      if (num_block_shutdown_tasks_posted_during_shutdown_ ==
          kMaxBlockShutdownTasksPostedDuringShutdown) {
        // Record the TaskScheduler.BlockShutdownTasksPostedDuringShutdown
        // histogram as soon as its upper bound is hit. That way, a value will
        // be recorded even if an infinite number of BLOCK_SHUTDOWN tasks are
        // posted, preventing shutdown to complete.
        RecordNumBlockShutdownTasksPostedDuringShutdown(
            num_block_shutdown_tasks_posted_during_shutdown_);
      }
    }

    // A BLOCK_SHUTDOWN task is allowed to be posted iff shutdown hasn't
    // completed.
    return true;
  }

  // A non BLOCK_SHUTDOWN task is allowed to be posted iff shutdown hasn't
  // started.
  return !shutdown_cv_;
}

bool TaskTracker::BeforeRunTask(TaskShutdownBehavior shutdown_behavior) {
  AutoSchedulerLock auto_lock(lock_);

  if (shutdown_completed_) {
    // Trying to run a BLOCK_SHUTDOWN task after shutdown has completed is
    // unexpected as it either shouldn't have been posted if shutdown completed
    // or should be blocking shutdown if it was posted before it did.
    DCHECK_NE(shutdown_behavior, TaskShutdownBehavior::BLOCK_SHUTDOWN);

    // A worker might extract a non BLOCK_SHUTDOWN task from a PriorityQueue
    // after shutdown. It shouldn't be allowed to run it.
    return false;
  }

  switch (shutdown_behavior) {
    case TaskShutdownBehavior::BLOCK_SHUTDOWN:
      DCHECK_GT(num_tasks_blocking_shutdown_, 0U);
      return true;

    case TaskShutdownBehavior::SKIP_ON_SHUTDOWN:
      if (shutdown_cv_)
        return false;

      // SKIP_ON_SHUTDOWN tasks block shutdown while they are running.
      ++num_tasks_blocking_shutdown_;
      return true;

    case TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN:
      return !shutdown_cv_;
  }

  NOTREACHED();
  return false;
}

void TaskTracker::AfterRunTask(TaskShutdownBehavior shutdown_behavior) {
  if (shutdown_behavior == TaskShutdownBehavior::BLOCK_SHUTDOWN ||
      shutdown_behavior == TaskShutdownBehavior::SKIP_ON_SHUTDOWN) {
    AutoSchedulerLock auto_lock(lock_);
    DCHECK_GT(num_tasks_blocking_shutdown_, 0U);
    --num_tasks_blocking_shutdown_;
    if (num_tasks_blocking_shutdown_ == 0 && shutdown_cv_)
      shutdown_cv_->Signal();
  }
}

}  // namespace internal
}  // namespace base
