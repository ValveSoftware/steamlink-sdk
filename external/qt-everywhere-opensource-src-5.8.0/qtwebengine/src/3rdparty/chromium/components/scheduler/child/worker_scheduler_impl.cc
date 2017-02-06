// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/worker_scheduler_impl.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"

namespace scheduler {

WorkerSchedulerImpl::WorkerSchedulerImpl(
    scoped_refptr<SchedulerTqmDelegate> main_task_runner)
    : helper_(main_task_runner,
              "worker.scheduler",
              TRACE_DISABLED_BY_DEFAULT("worker.scheduler"),
              TRACE_DISABLED_BY_DEFAULT("worker.scheduler.debug")),
      idle_helper_(&helper_,
                   this,
                   "worker.scheduler",
                   TRACE_DISABLED_BY_DEFAULT("worker.scheduler"),
                   "WorkerSchedulerIdlePeriod",
                   base::TimeDelta::FromMilliseconds(300)) {
  initialized_ = false;
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

WorkerSchedulerImpl::~WorkerSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

void WorkerSchedulerImpl::Init() {
  initialized_ = true;
  idle_helper_.EnableLongIdlePeriod();
}

scoped_refptr<TaskQueue> WorkerSchedulerImpl::DefaultTaskRunner() {
  DCHECK(initialized_);
  return helper_.DefaultTaskRunner();
}

scoped_refptr<SingleThreadIdleTaskRunner>
WorkerSchedulerImpl::IdleTaskRunner() {
  DCHECK(initialized_);
  return idle_helper_.IdleTaskRunner();
}

bool WorkerSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  DCHECK(initialized_);
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
}

bool WorkerSchedulerImpl::ShouldYieldForHighPriorityWork() {
  // We don't consider any work as being high priority on workers.
  return false;
}

void WorkerSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper_.AddTaskObserver(task_observer);
}

void WorkerSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper_.RemoveTaskObserver(task_observer);
}

void WorkerSchedulerImpl::Shutdown() {
  DCHECK(initialized_);
  helper_.Shutdown();
}

SchedulerHelper* WorkerSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

bool WorkerSchedulerImpl::CanEnterLongIdlePeriod(base::TimeTicks,
                                                 base::TimeDelta*) {
  return true;
}

base::TimeTicks WorkerSchedulerImpl::CurrentIdleTaskDeadlineForTesting() const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

}  // namespace scheduler
