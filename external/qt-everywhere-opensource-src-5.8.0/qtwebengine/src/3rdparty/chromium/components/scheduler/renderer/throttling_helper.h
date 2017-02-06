// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_THROTTLING_HELPER_H_
#define COMPONENTS_SCHEDULER_RENDERER_THROTTLING_HELPER_H_

#include <set>

#include "base/macros.h"
#include "components/scheduler/base/cancelable_closure_holder.h"
#include "components/scheduler/base/time_domain.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebViewScheduler.h"

namespace scheduler {

class RendererSchedulerImpl;
class ThrottledTimeDomain;
class WebFrameSchedulerImpl;

class SCHEDULER_EXPORT ThrottlingHelper : public TimeDomain::Observer {
 public:
  ThrottlingHelper(RendererSchedulerImpl* renderer_scheduler,
                   const char* tracing_category);

  ~ThrottlingHelper() override;

  // TimeDomain::Observer implementation:
  void OnTimeDomainHasImmediateWork() override;
  void OnTimeDomainHasDelayedWork() override;

  // The purpose of this method is to make sure throttling doesn't conflict with
  // enabling/disabling the queue for policy reasons.
  // If |task_queue| is throttled then the ThrottlingHelper remembers the
  // |enabled| setting.  In addition if |enabled| is false then the queue is
  // immediatly disabled.  Otherwise if |task_queue| not throttled then
  // TaskQueue::SetEnabled(enabled) is called.
  void SetQueueEnabled(TaskQueue* task_queue, bool enabled);

  // Increments the throttled refcount and causes |task_queue| to be throttled
  // if its not already throttled.
  void IncreaseThrottleRefCount(TaskQueue* task_queue);

  // If the refcouint is non-zero it's decremented.  If the throttled refcount
  // becomes zero then |task_queue| is unthrottled.  If the refcount was already
  // zero this function does nothing.
  void DecreaseThrottleRefCount(TaskQueue* task_queue);

  // Removes |task_queue| from |throttled_queues_|.
  void UnregisterTaskQueue(TaskQueue* task_queue);

  const ThrottledTimeDomain* time_domain() const { return time_domain_.get(); }

  static base::TimeTicks ThrottledRunTime(base::TimeTicks unthrottled_runtime);

  const scoped_refptr<TaskQueue>& task_runner() const { return task_runner_; }

 private:
  struct Metadata {
    Metadata() : throttling_ref_count(0), enabled(false) {}

    Metadata(size_t ref_count, bool is_enabled)
        : throttling_ref_count(ref_count), enabled(is_enabled) {}

    size_t throttling_ref_count;
    bool enabled;
  };
  using TaskQueueMap = std::map<TaskQueue*, Metadata>;

  void PumpThrottledTasks();

  // Note |unthrottled_runtime| might be in the past. When this happens we
  // compute the delay to the next runtime based on now rather than
  // unthrottled_runtime.
  void MaybeSchedulePumpThrottledTasksLocked(
      const tracked_objects::Location& from_here,
      base::TimeTicks now,
      base::TimeTicks unthrottled_runtime);

  TaskQueueMap throttled_queues_;
  base::Closure forward_immediate_work_closure_;
  scoped_refptr<TaskQueue> task_runner_;
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED
  base::TickClock* tick_clock_;                // NOT OWNED
  const char* tracing_category_;               // NOT OWNED
  std::unique_ptr<ThrottledTimeDomain> time_domain_;

  CancelableClosureHolder suspend_timers_when_backgrounded_closure_;
  base::TimeTicks pending_pump_throttled_tasks_runtime_;

  base::WeakPtrFactory<ThrottlingHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingHelper);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_THROTTLING_HELPER_H_
