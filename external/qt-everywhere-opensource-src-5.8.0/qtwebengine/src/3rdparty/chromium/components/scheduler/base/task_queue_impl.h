// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
#define CONTENT_RENDERER_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/pending_task.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/enqueue_order.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {
class LazyNow;
class TimeDomain;
class TaskQueueManager;

namespace internal {
class WorkQueue;
class WorkQueueSets;

class SCHEDULER_EXPORT TaskQueueImpl final : public TaskQueue {
 public:
  TaskQueueImpl(TaskQueueManager* task_queue_manager,
                TimeDomain* time_domain,
                const Spec& spec,
                const char* disabled_by_default_tracing_category,
                const char* disabled_by_default_verbose_tracing_category);

  class SCHEDULER_EXPORT Task : public base::PendingTask {
   public:
    Task();
    Task(const tracked_objects::Location& posted_from,
         const base::Closure& task,
         base::TimeTicks desired_run_time,
         EnqueueOrder sequence_number,
         bool nestable);

    Task(const tracked_objects::Location& posted_from,
         const base::Closure& task,
         base::TimeTicks desired_run_time,
         EnqueueOrder sequence_number,
         bool nestable,
         EnqueueOrder enqueue_order);

    EnqueueOrder enqueue_order() const {
#ifndef NDEBUG
      DCHECK(enqueue_order_set_);
#endif
      return enqueue_order_;
    }

    void set_enqueue_order(EnqueueOrder enqueue_order) {
#ifndef NDEBUG
      DCHECK(!enqueue_order_set_);
      enqueue_order_set_ = true;
#endif
      enqueue_order_ = enqueue_order;
    }

#ifndef NDEBUG
    bool enqueue_order_set() const { return enqueue_order_set_; }
#endif

   private:
#ifndef NDEBUG
    bool enqueue_order_set_;
#endif
    // Similar to sequence number, but the |enqueue_order| is set by
    // EnqueueTasksLocked and is not initially defined for delayed tasks until
    // they are enqueued on the |immediate_incoming_queue_|.
    EnqueueOrder enqueue_order_;
  };

  // TaskQueue implementation.
  void UnregisterTaskQueue() override;
  bool RunsTasksOnCurrentThread() const override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;

  void SetQueueEnabled(bool enabled) override;
  bool IsQueueEnabled() const override;
  bool IsEmpty() const override;
  bool HasPendingImmediateWork() const override;
  bool NeedsPumping() const override;
  void SetQueuePriority(QueuePriority priority) override;
  QueuePriority GetQueuePriority() const override;
  void PumpQueue(LazyNow* lazy_now, bool may_post_dowork) override;
  void SetPumpPolicy(PumpPolicy pump_policy) override;
  PumpPolicy GetPumpPolicy() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void SetTimeDomain(TimeDomain* time_domain) override;
  TimeDomain* GetTimeDomain() const override;
  void SetBlameContext(base::trace_event::BlameContext* blame_context) override;

  void UpdateImmediateWorkQueue(bool should_trigger_wakeup,
                                const Task* previous_task);
  void UpdateDelayedWorkQueue(LazyNow* lazy_now,
                              bool should_trigger_wakeup,
                              const Task* previous_task);

  WakeupPolicy wakeup_policy() const {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return wakeup_policy_;
  }

  const char* GetName() const override;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  bool GetQuiescenceMonitored() const { return should_monitor_quiescence_; }
  bool GetShouldNotifyObservers() const {
    return should_notify_observers_;
  }

  void NotifyWillProcessTask(const base::PendingTask& pending_task);
  void NotifyDidProcessTask(const base::PendingTask& pending_task);

  // Can be called on any thread.
  static const char* PumpPolicyToString(TaskQueue::PumpPolicy pump_policy);

  // Can be called on any thread.
  static const char* WakeupPolicyToString(
      TaskQueue::WakeupPolicy wakeup_policy);

  // Can be called on any thread.
  static const char* PriorityToString(TaskQueue::QueuePriority priority);

  WorkQueue* delayed_work_queue() {
    return main_thread_only().delayed_work_queue.get();
  }

  const WorkQueue* delayed_work_queue() const {
    return main_thread_only().delayed_work_queue.get();
  }

  WorkQueue* immediate_work_queue() {
    return main_thread_only().immediate_work_queue.get();
  }

  const WorkQueue* immediate_work_queue() const {
    return main_thread_only().immediate_work_queue.get();
  }

  bool should_report_when_execution_blocked() const {
    return should_report_when_execution_blocked_;
  }

 private:
  friend class WorkQueue;

  enum class TaskType {
    NORMAL,
    NON_NESTABLE,
  };

  struct AnyThread {
    AnyThread(TaskQueueManager* task_queue_manager,
              PumpPolicy pump_policy,
              TimeDomain* time_domain);
    ~AnyThread();

    // TaskQueueManager, PumpPolicy and TimeDomain are maintained in two copies:
    // inside AnyThread and inside MainThreadOnly. They can be changed only from
    // main thread, so it should be locked before accessing from other threads.
    TaskQueueManager* task_queue_manager;
    PumpPolicy pump_policy;
    TimeDomain* time_domain;

    std::queue<Task> immediate_incoming_queue;
  };

  struct MainThreadOnly {
    MainThreadOnly(TaskQueueManager* task_queue_manager,
                   PumpPolicy pump_policy,
                   TaskQueueImpl* task_queue,
                   TimeDomain* time_domain);
    ~MainThreadOnly();

    // Another copy of TaskQueueManager, PumpPolicy and TimeDomain for lock-free
    // access from the main thread. See description inside struct AnyThread for
    // details.
    TaskQueueManager* task_queue_manager;
    PumpPolicy pump_policy;
    TimeDomain* time_domain;

    std::unique_ptr<WorkQueue> delayed_work_queue;
    std::unique_ptr<WorkQueue> immediate_work_queue;
    std::priority_queue<Task> delayed_incoming_queue;
    base::ObserverList<base::MessageLoop::TaskObserver> task_observers;
    size_t set_index;
    bool is_enabled;
    base::trace_event::BlameContext* blame_context;  // Not owned.
  };

  ~TaskQueueImpl() override;

  bool PostImmediateTaskImpl(const tracked_objects::Location& from_here,
                             const base::Closure& task,
                             TaskType task_type);
  bool PostDelayedTaskImpl(const tracked_objects::Location& from_here,
                           const base::Closure& task,
                           base::TimeDelta delay,
                           TaskType task_type);

  // Push the task onto the |delayed_incoming_queue|. Lock-free main thread
  // only fast path.
  void PushOntoDelayedIncomingQueueFromMainThread(const Task& pending_task,
                                                  base::TimeTicks now);

  // Push the task onto the |delayed_incoming_queue|.  Slow path from other
  // threads.
  void PushOntoDelayedIncomingQueueLocked(const Task& pending_task);

  void ScheduleDelayedWorkTask(const Task& pending_task);

  // Enqueues any delayed tasks which should be run now on the
  // |delayed_work_queue|.  Must be called from the main thread.
  void MoveReadyDelayedTasksToDelayedWorkQueue(LazyNow* lazy_now);

  void MoveReadyImmediateTasksToImmediateWorkQueueLocked();

  // Note this does nothing if its not called from the main thread.
  void PumpQueueLocked(LazyNow* lazy_now, bool may_post_dowork);

  // Returns true if |task| is older than the oldest incoming immediate task.
  // NOTE |any_thread_lock_| must be locked.
  bool TaskIsOlderThanQueuedImmediateTasksLocked(const Task* task);

  // Returns true if |task| is older than the oldest delayed task.  Must be
  // called from the main thread.
  bool TaskIsOlderThanQueuedDelayedTasks(const Task* task);

  // NOTE |any_thread_lock_| must be locked.
  bool ShouldAutoPumpImmediateQueueLocked(bool should_trigger_wakeup,
                                          const Task* previous_task);

  //  Must be called from the main thread.
  bool ShouldAutoPumpDelayedQueue(bool should_trigger_wakeup,
                                  const Task* previous_task);

  // Push the task onto the |immediate_incoming_queue| and for auto pumped
  // queues it calls MaybePostDoWorkOnMainRunner if the Incoming queue was
  // empty.
  void PushOntoImmediateIncomingQueueLocked(const Task& pending_task);

  void TraceQueueSize(bool is_locked) const;
  static void QueueAsValueInto(const std::queue<Task>& queue,
                               base::trace_event::TracedValue* state);
  static void QueueAsValueInto(const std::priority_queue<Task>& queue,
                               base::trace_event::TracedValue* state);
  static void TaskAsValueInto(const Task& task,
                              base::trace_event::TracedValue* state);

  const base::PlatformThreadId thread_id_;

  mutable base::Lock any_thread_lock_;
  AnyThread any_thread_;
  struct AnyThread& any_thread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& any_thread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  const char* name_;
  const char* disabled_by_default_tracing_category_;
  const char* disabled_by_default_verbose_tracing_category_;

  base::ThreadChecker main_thread_checker_;
  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return main_thread_only_;
  }

  const WakeupPolicy wakeup_policy_;
  const bool should_monitor_quiescence_;
  const bool should_notify_observers_;
  const bool should_report_when_execution_blocked_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueImpl);
};

}  // namespace internal
}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
