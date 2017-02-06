// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_H_
#define COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class BlameContext;
}
}

namespace scheduler {
class LazyNow;
class TimeDomain;

class SCHEDULER_EXPORT TaskQueue : public base::SingleThreadTaskRunner {
 public:
  TaskQueue() {}

  // Unregisters the task queue after which no tasks posted to it will run and
  // the TaskQueueManager's reference to it will be released soon.
  virtual void UnregisterTaskQueue() = 0;

  enum QueuePriority {
    // Queues with control priority will run before any other queue, and will
    // explicitly starve other queues. Typically this should only be used for
    // private queues which perform control operations.
    CONTROL_PRIORITY,
    // Queues with high priority will be selected preferentially over normal or
    // best effort queues. The selector will ensure that high priority queues
    // cannot completely starve normal priority queues.
    HIGH_PRIORITY,
    // Queues with normal priority are the default.
    NORMAL_PRIORITY,
    // Queues with best effort priority will only be run if all other queues are
    // empty. They can be starved by the other queues.
    BEST_EFFORT_PRIORITY,
    // Must be the last entry.
    QUEUE_PRIORITY_COUNT,
    FIRST_QUEUE_PRIORITY = CONTROL_PRIORITY,
  };

  // Keep TaskQueue::PumpPolicyToString in sync with this enum.
  enum class PumpPolicy {
    // Tasks posted to an incoming queue with an AUTO pump policy will be
    // automatically scheduled for execution or transferred to the work queue
    // automatically.
    AUTO,
    // Tasks posted to an incoming queue with an AFTER_WAKEUP pump policy
    // will be scheduled for execution or transferred to the work queue
    // automatically but only after another queue has executed a task.
    AFTER_WAKEUP,
    // Tasks posted to an incoming queue with a MANUAL will not be
    // automatically scheduled for execution or transferred to the work queue.
    // Instead, the selector should call PumpQueue() when necessary to bring
    // in new tasks for execution.
    MANUAL,
    // Must be last entry.
    PUMP_POLICY_COUNT,
    FIRST_PUMP_POLICY = AUTO,
  };

  // Keep TaskQueue::WakeupPolicyToString in sync with this enum.
  enum class WakeupPolicy {
    // Tasks run on a queue with CAN_WAKE_OTHER_QUEUES wakeup policy can
    // cause queues with the AFTER_WAKEUP PumpPolicy to be woken up.
    CAN_WAKE_OTHER_QUEUES,
    // Tasks run on a queue with DONT_WAKE_OTHER_QUEUES won't cause queues
    // with the AFTER_WAKEUP PumpPolicy to be woken up.
    DONT_WAKE_OTHER_QUEUES,
    // Must be last entry.
    WAKEUP_POLICY_COUNT,
    FIRST_WAKEUP_POLICY = CAN_WAKE_OTHER_QUEUES,
  };

  // Options for constructing a TaskQueue. Once set the |name|,
  // |should_monitor_quiescence| and |wakeup_policy| are immutable. The
  // |pump_policy| can be mutated with |SetPumpPolicy()|.
  struct Spec {
    // Note |name| must have application lifetime.
    explicit Spec(const char* name)
        : name(name),
          should_monitor_quiescence(false),
          pump_policy(TaskQueue::PumpPolicy::AUTO),
          wakeup_policy(TaskQueue::WakeupPolicy::CAN_WAKE_OTHER_QUEUES),
          time_domain(nullptr),
          should_notify_observers(true),
          should_report_when_execution_blocked(false) {}

    Spec SetShouldMonitorQuiescence(bool should_monitor) {
      should_monitor_quiescence = should_monitor;
      return *this;
    }

    Spec SetPumpPolicy(PumpPolicy policy) {
      pump_policy = policy;
      return *this;
    }

    Spec SetWakeupPolicy(WakeupPolicy policy) {
      wakeup_policy = policy;
      return *this;
    }

    Spec SetShouldNotifyObservers(bool run_observers) {
      should_notify_observers = run_observers;
      return *this;
    }

    Spec SetTimeDomain(TimeDomain* domain) {
      time_domain = domain;
      return *this;
    }

    // See TaskQueueManager::Observer::OnTriedToExecuteBlockedTask.
    Spec SetShouldReportWhenExecutionBlocked(bool should_report) {
      should_report_when_execution_blocked = should_report;
      return *this;
    }

    const char* name;
    bool should_monitor_quiescence;
    TaskQueue::PumpPolicy pump_policy;
    TaskQueue::WakeupPolicy wakeup_policy;
    TimeDomain* time_domain;
    bool should_notify_observers;
    bool should_report_when_execution_blocked;
  };

  // Enable or disable task execution for this queue. NOTE this must be called
  // on the thread this TaskQueue was created by.
  virtual void SetQueueEnabled(bool enabled) = 0;

  // NOTE this must be called on the thread this TaskQueue was created by.
  virtual bool IsQueueEnabled() const = 0;

  // Returns true if the queue is completely empty.
  virtual bool IsEmpty() const = 0;

  // Returns true if the queue has work that's ready to execute now, or if it
  // would have if the queue was pumped. NOTE this must be called on the thread
  // this TaskQueue was created by.
  virtual bool HasPendingImmediateWork() const = 0;

  // Returns true if tasks can't run now but could if the queue was pumped.
  virtual bool NeedsPumping() const = 0;

  // Can be called on any thread.
  virtual const char* GetName() const = 0;

  // Set the priority of the queue to |priority|. NOTE this must be called on
  // the thread this TaskQueue was created by.
  virtual void SetQueuePriority(QueuePriority priority) = 0;

  // Returns the current queue priority.
  virtual QueuePriority GetQueuePriority() const = 0;

  // Set the pumping policy of the queue to |pump_policy|. NOTE this must be
  // called on the thread this TaskQueue was created by.
  virtual void SetPumpPolicy(PumpPolicy pump_policy) = 0;

  // Returns the current PumpPolicy. NOTE this must be called on the thread this
  // TaskQueue was created by.
  virtual PumpPolicy GetPumpPolicy() const = 0;

  // Reloads new tasks from the incoming queue into the work queue, regardless
  // of whether the work queue is empty or not. After this, the function ensures
  // that the tasks in the work queue, if any, are scheduled for execution.
  //
  // This function only needs to be called if automatic pumping is disabled.
  // By default automatic pumping is enabled for all queues. NOTE this must be
  // called on the thread this TaskQueue was created by.
  //
  // The |may_post_dowork| parameter controls whether or not PumpQueue calls
  // TaskQueueManager::MaybeScheduleImmediateWork.
  // TODO(alexclarke): Add a base::RunLoop observer so we can get rid of
  // |may_post_dowork|.
  virtual void PumpQueue(LazyNow* lazy_now, bool may_post_dowork) = 0;

  // These functions can only be called on the same thread that the task queue
  // manager executes its tasks on.
  virtual void AddTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) = 0;
  virtual void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) = 0;

  // Set the blame context which is entered and left while executing tasks from
  // this task queue. |blame_context| must be null or outlive this task queue.
  // Must be called on the thread this TaskQueue was created by.
  virtual void SetBlameContext(
      base::trace_event::BlameContext* blame_context) = 0;

  // Removes the task queue from the previous TimeDomain and adds it to
  // |domain|.  This is a moderately expensive operation.
  virtual void SetTimeDomain(TimeDomain* domain) = 0;

  // Returns the queue's current TimeDomain.  Can be called from any thread.
  virtual TimeDomain* GetTimeDomain() const = 0;

 protected:
  ~TaskQueue() override {}

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_H_
