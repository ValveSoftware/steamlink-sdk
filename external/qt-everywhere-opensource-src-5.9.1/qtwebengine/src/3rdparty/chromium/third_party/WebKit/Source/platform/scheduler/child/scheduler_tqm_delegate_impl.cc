// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/scheduler_tqm_delegate_impl.h"

#include <utility>

namespace blink {
namespace scheduler {

// static
scoped_refptr<SchedulerTqmDelegateImpl> SchedulerTqmDelegateImpl::Create(
    base::MessageLoop* message_loop,
    std::unique_ptr<base::TickClock> time_source) {
  return make_scoped_refptr(
      new SchedulerTqmDelegateImpl(message_loop, std::move(time_source)));
}

SchedulerTqmDelegateImpl::SchedulerTqmDelegateImpl(
    base::MessageLoop* message_loop,
    std::unique_ptr<base::TickClock> time_source)
    : message_loop_(message_loop),
      message_loop_task_runner_(message_loop->task_runner()),
      time_source_(std::move(time_source)) {}

SchedulerTqmDelegateImpl::~SchedulerTqmDelegateImpl() {
  RestoreDefaultTaskRunner();
}

void SchedulerTqmDelegateImpl::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  message_loop_->SetTaskRunner(task_runner);
}

void SchedulerTqmDelegateImpl::RestoreDefaultTaskRunner() {
  if (base::MessageLoop::current() == message_loop_)
    message_loop_->SetTaskRunner(message_loop_task_runner_);
}

bool SchedulerTqmDelegateImpl::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_task_runner_->PostDelayedTask(from_here, task, delay);
}

bool SchedulerTqmDelegateImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_task_runner_->PostNonNestableDelayedTask(from_here, task,
                                                               delay);
}

bool SchedulerTqmDelegateImpl::RunsTasksOnCurrentThread() const {
  return message_loop_task_runner_->RunsTasksOnCurrentThread();
}

bool SchedulerTqmDelegateImpl::IsNested() const {
  return message_loop_->IsNested();
}

void SchedulerTqmDelegateImpl::AddNestingObserver(
    base::MessageLoop::NestingObserver* observer) {
  message_loop_->AddNestingObserver(observer);
}

void SchedulerTqmDelegateImpl::RemoveNestingObserver(
    base::MessageLoop::NestingObserver* observer) {
  message_loop_->RemoveNestingObserver(observer);
}

base::TimeTicks SchedulerTqmDelegateImpl::NowTicks() {
  return time_source_->NowTicks();
}

}  // namespace scheduler
}  // namespace blink
