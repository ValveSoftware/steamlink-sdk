// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_

#include "base/macros.h"
#include "public/platform/scheduler/child/single_thread_idle_task_runner.h"
#include "public/platform/scheduler/child/worker_scheduler.h"
#include "public/platform/WebCommon.h"

namespace base {
class Thread;
}

namespace blink {
namespace scheduler {

class BLINK_PLATFORM_EXPORT CompositorWorkerScheduler
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
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_COMPOSITOR_WORKER_SCHEDULER_H_
