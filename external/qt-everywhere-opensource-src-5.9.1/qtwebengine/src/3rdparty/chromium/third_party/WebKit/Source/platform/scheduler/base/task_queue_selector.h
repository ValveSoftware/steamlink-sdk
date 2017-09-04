// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_SELECTOR_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_SELECTOR_H_

#include <stddef.h>

#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/pending_task.h"
#include "base/threading/thread_checker.h"
#include "platform/scheduler/base/work_queue_sets.h"

namespace blink {
namespace scheduler {
namespace internal {

// TaskQueueSelector is used by the SchedulerHelper to enable prioritization
// of particular task queues.
class BLINK_PLATFORM_EXPORT TaskQueueSelector {
 public:
  TaskQueueSelector();
  ~TaskQueueSelector();

  // Called to register a queue that can be selected. This function is called
  // on the main thread.
  void AddQueue(internal::TaskQueueImpl* queue);

  // The specified work will no longer be considered for selection. This
  // function is called on the main thread.
  void RemoveQueue(internal::TaskQueueImpl* queue);

  // Make |queue| eligible for selection. This function is called on the main
  // thread. Must only be called if |queue| is disabled.
  void EnableQueue(internal::TaskQueueImpl* queue);

  // Disable selection from |queue|. If task blocking is enabled for the queue,
  // Observer::OnTriedToSelectBlockedWorkQueue will be emitted if the
  // SelectWorkQueueToService tries to select this disabled queue for execution.
  // Must only be called if |queue| is enabled.
  void DisableQueue(internal::TaskQueueImpl* queue);

  // Called get or set the priority of |queue|.
  void SetQueuePriority(internal::TaskQueueImpl* queue,
                        TaskQueue::QueuePriority priority);

  // Called to choose the work queue from which the next task should be taken
  // and run. Return true if |out_work_queue| indicates the queue to service or
  // false to avoid running any task.
  //
  // This function is called on the main thread.
  bool SelectWorkQueueToService(WorkQueue** out_work_queue);

  // Serialize the selector state for tracing.
  void AsValueInto(base::trace_event::TracedValue* state) const;

  class BLINK_PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Called when |queue| transitions from disabled to enabled.
    virtual void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) = 0;

    // Called when the selector tried to select a task from a disabled work
    // queue. See TaskQueue::Spec::SetShouldReportWhenExecutionBlocked. A single
    // call to SelectWorkQueueToService will only result in up to one
    // blocking notification even if multiple disabled queues could have been
    // selected.
    virtual void OnTriedToSelectBlockedWorkQueue(
        internal::WorkQueue* work_queue) = 0;
  };

  // Called once to set the Observer. This function is called
  // on the main thread. If |observer| is null, then no callbacks will occur.
  void SetTaskQueueSelectorObserver(Observer* observer);

  // Returns true if all the enabled work queues are empty. Returns false
  // otherwise.
  bool EnabledWorkQueuesEmpty() const;

 protected:
  class BLINK_PLATFORM_EXPORT PrioritizingSelector {
   public:
    PrioritizingSelector(TaskQueueSelector* task_queue_selector,
                         const char* name);

    void ChangeSetIndex(internal::TaskQueueImpl* queue,
                        TaskQueue::QueuePriority priority);
    void AddQueue(internal::TaskQueueImpl* queue,
                  TaskQueue::QueuePriority priority);
    void RemoveQueue(internal::TaskQueueImpl* queue);

    bool SelectWorkQueueToService(TaskQueue::QueuePriority max_priority,
                                  WorkQueue** out_work_queue,
                                  bool* out_chose_delayed_over_immediate);

    WorkQueueSets* delayed_work_queue_sets() {
      return &delayed_work_queue_sets_;
    }
    WorkQueueSets* immediate_work_queue_sets() {
      return &immediate_work_queue_sets_;
    }

    const WorkQueueSets* delayed_work_queue_sets() const {
      return &delayed_work_queue_sets_;
    }
    const WorkQueueSets* immediate_work_queue_sets() const {
      return &immediate_work_queue_sets_;
    }

    bool ChooseOldestWithPriority(TaskQueue::QueuePriority priority,
                                  bool* out_chose_delayed_over_immediate,
                                  WorkQueue** out_work_queue) const;

#if DCHECK_IS_ON() || !defined(NDEBUG)
    bool CheckContainsQueueForTest(const internal::TaskQueueImpl* queue) const;
#endif

   private:
    bool ChooseOldestImmediateTaskWithPriority(
        TaskQueue::QueuePriority priority,
        WorkQueue** out_work_queue) const;

    bool ChooseOldestDelayedTaskWithPriority(TaskQueue::QueuePriority priority,
                                             WorkQueue** out_work_queue) const;

    // Return true if |out_queue| contains the queue with the oldest pending
    // task from the set of queues of |priority|, or false if all queues of that
    // priority are empty. In addition |out_chose_delayed_over_immediate| is set
    // to true iff we chose a delayed work queue in favour of an immediate work
    // queue.
    bool ChooseOldestImmediateOrDelayedTaskWithPriority(
        TaskQueue::QueuePriority priority,
        bool* out_chose_delayed_over_immediate,
        WorkQueue** out_work_queue) const;

    const TaskQueueSelector* task_queue_selector_;
    WorkQueueSets delayed_work_queue_sets_;
    WorkQueueSets immediate_work_queue_sets_;

    DISALLOW_COPY_AND_ASSIGN(PrioritizingSelector);
  };

  // Return true if |out_queue| contains the queue with the oldest pending task
  // from the set of queues of |priority|, or false if all queues of that
  // priority are empty. In addition |out_chose_delayed_over_immediate| is set
  // to true iff we chose a delayed work queue in favour of an immediate work
  // queue.  This method will force select an immediate task if those are being
  // starved by delayed tasks.
  void SetImmediateStarvationCountForTest(size_t immediate_starvation_count);

  PrioritizingSelector* enabled_selector_for_test() {
    return &enabled_selector_;
  }

 private:
  // Returns the priority which is next after |priority|.
  static TaskQueue::QueuePriority NextPriority(
      TaskQueue::QueuePriority priority);

  bool SelectWorkQueueToServiceInternal(WorkQueue** out_work_queue);

  // Called whenever the selector chooses a task queue for execution with the
  // priority |priority|.
  void DidSelectQueueWithPriority(TaskQueue::QueuePriority priority,
                                  bool chose_delayed_over_immediate);

  // No enabled queue could be selected, check if we could have chosen a
  // disabled (blocked) work queue instead.
  void TrySelectingBlockedQueue();

  // Check if we could have chosen a disabled (blocked) work queue instead.
  // |chosen_enabled_queue| is the enabled queue that got chosen.
  void TrySelectingBlockedQueueOverEnabledQueue(
      const WorkQueue& chosen_enabled_queue);

  // Number of high priority tasks which can be run before a normal priority
  // task should be selected to prevent starvation.
  // TODO(rmcilroy): Check if this is a good value.
  static const size_t kMaxHighPriorityStarvationTasks = 5;

  // Maximum number of delayed tasks tasks which can be run while there's a
  // waiting non-delayed task.
  static const size_t kMaxDelayedStarvationTasks = 3;

 private:
  base::ThreadChecker main_thread_checker_;

  PrioritizingSelector enabled_selector_;
  PrioritizingSelector blocked_selector_;
  size_t immediate_starvation_count_;
  size_t high_priority_starvation_count_;
  size_t num_blocked_queues_to_report_;

  Observer* task_queue_selector_observer_;  // NOT OWNED
  DISALLOW_COPY_AND_ASSIGN(TaskQueueSelector);
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_SELECTOR_H_
