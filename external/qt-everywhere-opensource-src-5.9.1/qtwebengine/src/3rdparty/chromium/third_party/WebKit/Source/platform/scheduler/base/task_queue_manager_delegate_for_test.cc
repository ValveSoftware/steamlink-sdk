// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_manager_delegate_for_test.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace blink {
namespace scheduler {

// static
scoped_refptr<TaskQueueManagerDelegateForTest>
TaskQueueManagerDelegateForTest::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> time_source) {
  return make_scoped_refptr(
      new TaskQueueManagerDelegateForTest(task_runner, std::move(time_source)));
}

TaskQueueManagerDelegateForTest::TaskQueueManagerDelegateForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> time_source)
    : task_runner_(task_runner), time_source_(std::move(time_source)) {}

TaskQueueManagerDelegateForTest::~TaskQueueManagerDelegateForTest() {}

bool TaskQueueManagerDelegateForTest::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(from_here, task, delay);
}

bool TaskQueueManagerDelegateForTest::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(from_here, task, delay);
}

bool TaskQueueManagerDelegateForTest::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

bool TaskQueueManagerDelegateForTest::IsNested() const {
  return false;
}

void TaskQueueManagerDelegateForTest::AddNestingObserver(
    base::MessageLoop::NestingObserver* observer) {}

void TaskQueueManagerDelegateForTest::RemoveNestingObserver(
    base::MessageLoop::NestingObserver* observer) {}

base::TimeTicks TaskQueueManagerDelegateForTest::NowTicks() {
  return time_source_->NowTicks();
}

}  // namespace scheduler
}  // namespace blink
