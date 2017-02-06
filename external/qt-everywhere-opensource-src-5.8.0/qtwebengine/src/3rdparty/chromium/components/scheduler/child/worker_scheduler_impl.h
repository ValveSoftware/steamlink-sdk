// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_WORKER_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_CHILD_WORKER_SCHEDULER_IMPL_H_

#include "base/macros.h"
#include "components/scheduler/child/idle_helper.h"
#include "components/scheduler/child/scheduler_helper.h"
#include "components/scheduler/child/worker_scheduler.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace scheduler {

class SchedulerTqmDelegate;

class SCHEDULER_EXPORT WorkerSchedulerImpl : public WorkerScheduler,
                                             public IdleHelper::Delegate {
 public:
  explicit WorkerSchedulerImpl(
      scoped_refptr<SchedulerTqmDelegate> main_task_runner);
  ~WorkerSchedulerImpl() override;

  // WorkerScheduler implementation:
  scoped_refptr<TaskQueue> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  bool ShouldYieldForHighPriorityWork() override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Init() override;
  void Shutdown() override;

  SchedulerHelper* GetSchedulerHelperForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;

 protected:
  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override {}
  void OnIdlePeriodEnded() override {}

 private:
  void MaybeStartLongIdlePeriod();

  SchedulerHelper helper_;
  IdleHelper idle_helper_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_WORKER_SCHEDULER_IMPL_H_
