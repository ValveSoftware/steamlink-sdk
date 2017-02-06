// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/single_thread_idle_task_runner.h"

#include "base/location.h"
#include "base/trace_event/blame_context.h"
#include "base/trace_event/trace_event.h"

namespace scheduler {

SingleThreadIdleTaskRunner::SingleThreadIdleTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> idle_priority_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> after_wakeup_task_runner,
    Delegate* delegate,
    const char* tracing_category)
    : idle_priority_task_runner_(idle_priority_task_runner),
      after_wakeup_task_runner_(after_wakeup_task_runner),
      delegate_(delegate),
      tracing_category_(tracing_category),
      blame_context_(nullptr),
      weak_factory_(this) {
  DCHECK(!idle_priority_task_runner_ ||
         idle_priority_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!after_wakeup_task_runner_ ||
         after_wakeup_task_runner_->RunsTasksOnCurrentThread());
  weak_scheduler_ptr_ = weak_factory_.GetWeakPtr();
}

SingleThreadIdleTaskRunner::~SingleThreadIdleTaskRunner() {
}

SingleThreadIdleTaskRunner::Delegate::Delegate() {
}

SingleThreadIdleTaskRunner::Delegate::~Delegate() {
}

bool SingleThreadIdleTaskRunner::RunsTasksOnCurrentThread() const {
  return idle_priority_task_runner_->RunsTasksOnCurrentThread();
}

void SingleThreadIdleTaskRunner::PostIdleTask(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
  delegate_->OnIdleTaskPosted();
  idle_priority_task_runner_->PostTask(
      from_here, base::Bind(&SingleThreadIdleTaskRunner::RunTask,
                            weak_scheduler_ptr_, idle_task));
}

void SingleThreadIdleTaskRunner::PostNonNestableIdleTask(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
  delegate_->OnIdleTaskPosted();
  idle_priority_task_runner_->PostNonNestableTask(
      from_here, base::Bind(&SingleThreadIdleTaskRunner::RunTask,
                            weak_scheduler_ptr_, idle_task));
}

void SingleThreadIdleTaskRunner::PostIdleTaskAfterWakeup(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
  // Don't signal posting of idle task to the delegate here, wait until the
  // after-wakeup task posts the real idle task.
  after_wakeup_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SingleThreadIdleTaskRunner::PostIdleTask,
                            weak_scheduler_ptr_, from_here, idle_task));
}

void SingleThreadIdleTaskRunner::RunTask(IdleTask idle_task) {
  base::TimeTicks deadline = delegate_->WillProcessIdleTask();
  TRACE_EVENT1(tracing_category_, "SingleThreadIdleTaskRunner::RunTask",
               "allotted_time_ms",
               (deadline - base::TimeTicks::Now()).InMillisecondsF());
  if (blame_context_)
    blame_context_->Enter();
  idle_task.Run(deadline);
  if (blame_context_)
    blame_context_->Leave();
  delegate_->DidProcessIdleTask();
}

void SingleThreadIdleTaskRunner::SetBlameContext(
    base::trace_event::BlameContext* blame_context) {
  blame_context_ = blame_context;
}

}  // namespace scheduler
