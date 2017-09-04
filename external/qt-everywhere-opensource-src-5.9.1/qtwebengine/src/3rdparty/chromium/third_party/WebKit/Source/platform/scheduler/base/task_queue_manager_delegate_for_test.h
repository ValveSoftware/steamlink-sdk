// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_

#include <memory>

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"

namespace blink {
namespace scheduler {

class TaskQueueManagerDelegateForTest : public TaskQueueManagerDelegate {
 public:
  static scoped_refptr<TaskQueueManagerDelegateForTest> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<base::TickClock> time_source);

  // NestableSingleThreadTaskRunner implementation
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  void AddNestingObserver(
      base::MessageLoop::NestingObserver* observer) override;
  void RemoveNestingObserver(
      base::MessageLoop::NestingObserver* observer) override;
  base::TimeTicks NowTicks() override;

 protected:
  ~TaskQueueManagerDelegateForTest() override;
  TaskQueueManagerDelegateForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<base::TickClock> time_source);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<base::TickClock> time_source_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManagerDelegateForTest);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_
