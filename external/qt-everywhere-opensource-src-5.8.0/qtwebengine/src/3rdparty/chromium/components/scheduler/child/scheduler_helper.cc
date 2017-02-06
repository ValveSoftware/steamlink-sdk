// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/scheduler_helper.h"

#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"

namespace scheduler {

SchedulerHelper::SchedulerHelper(
    scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate,
    const char* tracing_category,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : task_queue_manager_delegate_(task_queue_manager_delegate),
      task_queue_manager_(
          new TaskQueueManager(task_queue_manager_delegate,
                               tracing_category,
                               disabled_by_default_tracing_category,
                               disabled_by_default_verbose_tracing_category)),
      control_task_runner_(NewTaskQueue(
          TaskQueue::Spec("control_tq")
              .SetWakeupPolicy(TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES)
              .SetShouldNotifyObservers(false))),
      control_after_wakeup_task_runner_(NewTaskQueue(
          TaskQueue::Spec("control_after_wakeup_tq")
              .SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP)
              .SetWakeupPolicy(TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES)
              .SetShouldNotifyObservers(false))),
      default_task_runner_(NewTaskQueue(TaskQueue::Spec("default_tq")
                                            .SetShouldMonitorQuiescence(true))),
      observer_(nullptr),
      tracing_category_(tracing_category),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category) {
  control_task_runner_->SetQueuePriority(TaskQueue::CONTROL_PRIORITY);
  control_after_wakeup_task_runner_->SetQueuePriority(
      TaskQueue::CONTROL_PRIORITY);

  task_queue_manager_->SetWorkBatchSize(4);

  DCHECK(task_queue_manager_delegate_);
  task_queue_manager_delegate_->SetDefaultTaskRunner(
      default_task_runner_.get());
}

SchedulerHelper::~SchedulerHelper() {
  Shutdown();
}

void SchedulerHelper::Shutdown() {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->SetObserver(nullptr);
  task_queue_manager_.reset();
  task_queue_manager_delegate_->RestoreDefaultTaskRunner();
}

scoped_refptr<TaskQueue> SchedulerHelper::NewTaskQueue(
    const TaskQueue::Spec& spec) {
  DCHECK(task_queue_manager_.get());
  return task_queue_manager_->NewTaskQueue(spec);
}

scoped_refptr<TaskQueue> SchedulerHelper::DefaultTaskRunner() {
  CheckOnValidThread();
  return default_task_runner_;
}

scoped_refptr<TaskQueue> SchedulerHelper::ControlTaskRunner() {
  return control_task_runner_;
}

scoped_refptr<TaskQueue> SchedulerHelper::ControlAfterWakeUpTaskRunner() {
  return control_after_wakeup_task_runner_;
}

void SchedulerHelper::SetWorkBatchSizeForTesting(size_t work_batch_size) {
  CheckOnValidThread();
  DCHECK(task_queue_manager_.get());
  task_queue_manager_->SetWorkBatchSize(work_batch_size);
}

TaskQueueManager* SchedulerHelper::GetTaskQueueManagerForTesting() {
  CheckOnValidThread();
  return task_queue_manager_.get();
}

const scoped_refptr<SchedulerTqmDelegate>&
SchedulerHelper::scheduler_tqm_delegate() const {
  return task_queue_manager_delegate_;
}


bool SchedulerHelper::GetAndClearSystemIsQuiescentBit() {
  CheckOnValidThread();
  DCHECK(task_queue_manager_.get());
  return task_queue_manager_->GetAndClearSystemIsQuiescentBit();
}

void SchedulerHelper::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->AddTaskObserver(task_observer);
}

void SchedulerHelper::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->RemoveTaskObserver(task_observer);
}

void SchedulerHelper::SetObserver(Observer* observer) {
  CheckOnValidThread();
  observer_ = observer;
  DCHECK(task_queue_manager_);
  task_queue_manager_->SetObserver(this);
}

RealTimeDomain* SchedulerHelper::real_time_domain() const {
  CheckOnValidThread();
  DCHECK(task_queue_manager_);
  return task_queue_manager_->real_time_domain();
}

void SchedulerHelper::RegisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  DCHECK(task_queue_manager_);
  task_queue_manager_->RegisterTimeDomain(time_domain);
}

void SchedulerHelper::UnregisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->UnregisterTimeDomain(time_domain);
}

void SchedulerHelper::OnUnregisterTaskQueue(
    const scoped_refptr<TaskQueue>& queue) {
  if (observer_)
    observer_->OnUnregisterTaskQueue(queue);
}

void SchedulerHelper::OnTriedToExecuteBlockedTask(
    const TaskQueue& queue,
    const base::PendingTask& task) {
  if (observer_)
    observer_->OnTriedToExecuteBlockedTask(queue, task);
}

TaskQueue* SchedulerHelper::CurrentlyExecutingTaskQueue() const {
  if (!task_queue_manager_)
    return nullptr;
  return task_queue_manager_->currently_executing_task_queue();
}

}  // namespace scheduler
