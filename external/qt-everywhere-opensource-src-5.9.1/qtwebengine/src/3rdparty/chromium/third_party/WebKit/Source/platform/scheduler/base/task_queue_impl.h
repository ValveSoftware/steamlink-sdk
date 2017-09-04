// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/pending_task.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/scheduler/base/enqueue_order.h"
#include "platform/scheduler/base/intrusive_heap.h"
#include "public/platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {
class LazyNow;
class TimeDomain;
class TaskQueueManager;

namespace internal {
class WorkQueue;
class WorkQueueSets;

// TaskQueueImpl has four main queues:
//
// Immediate (non-delayed) tasks:
//    immediate_incoming_queue - PostTask enqueues tasks here
//    immediate_work_queue
//
// Delayed tasks
//    delayed_incoming_queue - PostDelayedTask enqueues tasks here
//    delayed_work_queue
//
// The immediate_incoming_queue can be accessed from any thread, the other
// queues are main-thread only. To reduce the overhead of locking,
// immediate_work_queue is swapped with immediate_incoming_queue when
// immediate_work_queue becomes empty.
//
// Delayed tasks are initially posted to delayed_incoming_queue and a wakeup
// is scheduled with the TimeDomain.  When the delay has elapsed, the TimeDomain
// calls UpdateDelayedWorkQueue and ready delayed tasks are moved into the
// delayed_work_queue.  Note the EnqueueOrder (used for ordering) for a delayed
// task is not set until it's moved into the delayed_work_queue.
//
// TaskQueueImpl uses the WorkQueueSets and the TaskQueueSelector to implement
// prioritization. Task selection is done by the TaskQueueSelector and when a
// queue is selected, it round-robins between the immediate_work_queue and
// delayed_work_queue.  The reason for this is we want to make sure delayed
// tasks (normally the most common type) don't starve out immediate work.
class BLINK_PLATFORM_EXPORT TaskQueueImpl final : public TaskQueue {
 public:
  TaskQueueImpl(TaskQueueManager* task_queue_manager,
                TimeDomain* time_domain,
                const Spec& spec,
                const char* disabled_by_default_tracing_category,
                const char* disabled_by_default_verbose_tracing_category);

  class BLINK_PLATFORM_EXPORT Task : public base::PendingTask {
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
    // Similar to sequence number, but ultimately the |enqueue_order_| is what
    // the scheduler uses for task ordering. For immediate tasks |enqueue_order|
    // is set when posted, but for delayed tasks it's not defined until they are
    // enqueued on the |delayed_work_queue_|. This is because otherwise delayed
    // tasks could run before an immediate task posted after the delayed task.
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
  size_t GetNumberOfPendingTasks() const override;
  bool HasPendingImmediateWork() const override;
  base::Optional<base::TimeTicks> GetNextScheduledWakeUp() override;
  void SetQueuePriority(QueuePriority priority) override;
  QueuePriority GetQueuePriority() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void SetTimeDomain(TimeDomain* time_domain) override;
  TimeDomain* GetTimeDomain() const override;
  void SetBlameContext(base::trace_event::BlameContext* blame_context) override;
  void InsertFence(InsertFencePosition position) override;
  void RemoveFence() override;
  bool BlockedByFence() const override;

  // If this returns false then future updates for this queue are not needed
  // unless requested.
  bool MaybeUpdateImmediateWorkQueues();

  const char* GetName() const override;
  QueueType GetQueueType() const override;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  bool GetQuiescenceMonitored() const { return should_monitor_quiescence_; }
  bool GetShouldNotifyObservers() const { return should_notify_observers_; }

  void NotifyWillProcessTask(const base::PendingTask& pending_task);
  void NotifyDidProcessTask(const base::PendingTask& pending_task);

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

  // Enqueues any delayed tasks which should be run now on the
  // |delayed_work_queue|. It also schedules the next wake up with the
  // TimeDomain. Must be called from the main thread.
  void WakeUpForDelayedWork(LazyNow* lazy_now);

  base::TimeTicks scheduled_time_domain_wakeup() const {
    return main_thread_only().scheduled_time_domain_wakeup;
  }

  void set_scheduled_time_domain_wakeup(
      base::TimeTicks scheduled_time_domain_wakeup) {
    main_thread_only().scheduled_time_domain_wakeup =
        scheduled_time_domain_wakeup;
  }

  HeapHandle heap_handle() const { return main_thread_only().heap_handle; }

  void set_heap_handle(HeapHandle heap_handle) {
    main_thread_only().heap_handle = heap_handle;
  }

  EnqueueOrder GetFenceForTest() const;

 private:
  friend class WorkQueue;
  friend class WorkQueueTest;

  enum class TaskType {
    NORMAL,
    NON_NESTABLE,
  };

  struct AnyThread {
    AnyThread(TaskQueueManager* task_queue_manager, TimeDomain* time_domain);
    ~AnyThread();

    // TaskQueueManager and TimeDomain are maintained in two copies:
    // inside AnyThread and inside MainThreadOnly. They can be changed only from
    // main thread, so it should be locked before accessing from other threads.
    TaskQueueManager* task_queue_manager;
    TimeDomain* time_domain;

    std::queue<Task> immediate_incoming_queue;
  };

  struct MainThreadOnly {
    MainThreadOnly(TaskQueueManager* task_queue_manager,
                   TaskQueueImpl* task_queue,
                   TimeDomain* time_domain);
    ~MainThreadOnly();

    // Another copy of TaskQueueManager and TimeDomain for lock-free access from
    // the main thread. See description inside struct AnyThread for details.
    TaskQueueManager* task_queue_manager;
    TimeDomain* time_domain;

    std::unique_ptr<WorkQueue> delayed_work_queue;
    std::unique_ptr<WorkQueue> immediate_work_queue;
    std::priority_queue<Task> delayed_incoming_queue;
    base::ObserverList<base::MessageLoop::TaskObserver> task_observers;
    size_t set_index;
    HeapHandle heap_handle;
    bool is_enabled;
    base::trace_event::BlameContext* blame_context;  // Not owned.
    EnqueueOrder current_fence;
    base::TimeTicks scheduled_time_domain_wakeup;
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
  void PushOntoDelayedIncomingQueueFromMainThread(Task pending_task,
                                                  base::TimeTicks now);

  // Push the task onto the |delayed_incoming_queue|.  Slow path from other
  // threads.
  void PushOntoDelayedIncomingQueueLocked(Task pending_task);

  void ScheduleDelayedWorkTask(Task pending_task);

  void MoveReadyImmediateTasksToImmediateWorkQueueLocked();

  // Push the task onto the |immediate_incoming_queue| and for auto pumped
  // queues it calls MaybePostDoWorkOnMainRunner if the Incoming queue was
  // empty.
  void PushOntoImmediateIncomingQueueLocked(
      const tracked_objects::Location& posted_from,
      const base::Closure& task,
      base::TimeTicks desired_run_time,
      EnqueueOrder sequence_number,
      bool nestable);

  // As BlockedByFence but safe to be called while locked.
  bool BlockedByFenceLocked() const;

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

  const QueueType type_;
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

  const bool should_monitor_quiescence_;
  const bool should_notify_observers_;
  const bool should_report_when_execution_blocked_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueImpl);
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
