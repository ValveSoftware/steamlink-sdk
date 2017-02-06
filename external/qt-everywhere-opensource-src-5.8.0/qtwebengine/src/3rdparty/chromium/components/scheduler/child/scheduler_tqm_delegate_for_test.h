// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_TQM_DELEGATE_FOR_TEST_H_
#define CONTENT_RENDERER_SCHEDULER_TQM_DELEGATE_FOR_TEST_H_

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"

namespace scheduler {

class TaskQueueManagerDelegateForTest;

class SchedulerTqmDelegateForTest : public SchedulerTqmDelegate {
 public:
  static scoped_refptr<SchedulerTqmDelegateForTest> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<base::TickClock> time_source);

  // SchedulerTqmDelegate implementation
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void RestoreDefaultTaskRunner() override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  base::TimeTicks NowTicks() override;

  base::SingleThreadTaskRunner* default_task_runner() const {
    return default_task_runner_.get();
  }

 protected:
  ~SchedulerTqmDelegateForTest() override;

 private:
  SchedulerTqmDelegateForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<base::TickClock> time_source);

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  scoped_refptr<TaskQueueManagerDelegateForTest> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerTqmDelegateForTest);
};

}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_TQM_DELEGATE_FOR_TEST_H_
