// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_

#include "base/macros.h"
#include "components/scheduler/child/single_thread_idle_task_runner.h"
#include "components/scheduler/child/worker_scheduler.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
class Thread;
}

namespace scheduler {

class SCHEDULER_EXPORT CompositorWorkerScheduler
    : public WorkerScheduler,
      public SingleThreadIdleTaskRunner::Delegate {
 public:
  explicit CompositorWorkerScheduler(base::Thread* thread);
  ~CompositorWorkerScheduler() override;

  // WorkerScheduler:
  void Init() override;

  // ChildScheduler:
  scoped_refptr<TaskQueue> DefaultTaskRunner() override;
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> IdleTaskRunner()
      override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;

  // SingleThreadIdleTaskRunner::Delegate:
  void OnIdleTaskPosted() override;
  base::TimeTicks WillProcessIdleTask() override;
  void DidProcessIdleTask() override;

 private:
  base::Thread* thread_;

  DISALLOW_COPY_AND_ASSIGN(CompositorWorkerScheduler);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_
