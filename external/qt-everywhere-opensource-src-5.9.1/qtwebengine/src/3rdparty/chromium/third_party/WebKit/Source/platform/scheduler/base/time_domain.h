// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TIME_DOMAIN_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TIME_DOMAIN_H_

#include <map>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "platform/scheduler/base/intrusive_heap.h"
#include "platform/scheduler/base/lazy_now.h"
#include "platform/scheduler/base/task_queue_impl.h"

namespace blink {
namespace scheduler {
namespace internal {
class TaskQueueImpl;
}  // internal
class TaskQueueManager;

// The TimeDomain's job is to wake task queues up when their next delayed tasks
// are due to fire. TaskQueues request a wake up via ScheduleDelayedWork, when
// the wake up is due the TimeDomain calls TaskQueue::WakeUpForDelayedWork.
// The TimeDomain communicates with the TaskQueueManager to actually schedule
// the wake-ups on the underlying base::MessageLoop. Various levels of de-duping
// are employed to prevent unnecessary posting of TaskQueueManager::DoWork.
//
// Note the TimeDomain only knows about the first wakeup per queue, it's the
// responsibility of TaskQueueImpl to keep the time domain up to date if this
// changes.
class BLINK_PLATFORM_EXPORT TimeDomain {
 public:
  class BLINK_PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Called when an empty TaskQueue registered with this TimeDomain has a task
    // enqueued.
    // |task_queue| - task queue which has immediate work scheduled.
    virtual void OnTimeDomainHasImmediateWork(TaskQueue* task_queue) = 0;

    // Called when a TaskQueue registered with this TimeDomain has a delayed
    // task enqueued.
    // |task_queue| - task queue which has delayed work scheduled.
    virtual void OnTimeDomainHasDelayedWork(TaskQueue* task_queue) = 0;
  };

  explicit TimeDomain(Observer* observer);
  virtual ~TimeDomain();

  // Returns a LazyNow that evaluate this TimeDomain's Now.  Can be called from
  // any thread.
  // TODO(alexclarke): Make this main thread only.
  virtual LazyNow CreateLazyNow() const = 0;

  // Evaluate this TimeDomain's Now. Can be called from any thread.
  virtual base::TimeTicks Now() const = 0;

  // Some TimeDomains support virtual time, this method tells us to advance time
  // if possible and return true if time was advanced.
  virtual bool MaybeAdvanceTime() = 0;

  // Returns the name of this time domain for tracing.
  virtual const char* GetName() const = 0;

  // If there is a scheduled delayed task, |out_time| is set to the scheduled
  // runtime for the next one and it returns true.  Returns false otherwise.
  bool NextScheduledRunTime(base::TimeTicks* out_time) const;

 protected:
  friend class internal::TaskQueueImpl;
  friend class TaskQueueManager;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  // Migrates |queue| from this time domain to |destination_time_domain|.
  // Main-thread only.
  void MigrateQueue(internal::TaskQueueImpl* queue,
                    TimeDomain* destination_time_domain);

  // If there is a scheduled delayed task, |out_task_queue| is set to the queue
  // the next task was posted to and it returns true.  Returns false otherwise.
  bool NextScheduledTaskQueue(TaskQueue** out_task_queue) const;

  // Adds |queue| to the set of task queues that UpdateWorkQueues calls
  // UpdateWorkQueue on.
  void RegisterAsUpdatableTaskQueue(internal::TaskQueueImpl* queue);

  // Schedules a call to TaskQueueImpl::WakeUpForDelayedWork when this
  // TimeDomain reaches |delayed_run_time|.  This supersedes any previously
  // registered wakeup for |queue|.
  void ScheduleDelayedWork(internal::TaskQueueImpl* queue,
                           base::TimeTicks delayed_run_time,
                           base::TimeTicks now);

  // Registers the |queue|.
  void RegisterQueue(internal::TaskQueueImpl* queue);

  // Removes |queue| from the set of task queues that UpdateWorkQueues calls
  // UpdateWorkQueue on. Returns true if |queue| was updatable.
  bool UnregisterAsUpdatableTaskQueue(internal::TaskQueueImpl* queue);

  // Removes |queue| from all internal data structures.
  void UnregisterQueue(internal::TaskQueueImpl* queue);

  // Updates active queues associated with this TimeDomain.
  void UpdateWorkQueues(LazyNow lazy_now);

  // Called by the TaskQueueManager when the TimeDomain is registered.
  virtual void OnRegisterWithTaskQueueManager(
      TaskQueueManager* task_queue_manager) = 0;

  // The implementaion will secedule task processing to run with |delay| with
  // respect to the TimeDomain's time source.  Always called on the main thread.
  // NOTE this is only called by ScheduleDelayedWork if the scheduled runtime
  // is sooner than any previously sheduled work or if there is no other
  // scheduled work.
  virtual void RequestWakeup(base::TimeTicks now, base::TimeDelta delay) = 0;

  // For implementation specific tracing.
  virtual void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const = 0;

  // Call TaskQueueImpl::UpdateDelayedWorkQueue for each queue where the delay
  // has elapsed.
  void WakeupReadyDelayedQueues(LazyNow* lazy_now);

  size_t NumberOfScheduledWakeups() const {
    return delayed_wakeup_queue_.size();
  }

 private:
  void MoveNewlyUpdatableQueuesIntoUpdatableQueueSet();

  struct DelayedWakeup {
    base::TimeTicks time;
    internal::TaskQueueImpl* queue;

    bool operator<=(const DelayedWakeup& other) const {
      if (time == other.time)
        return queue <= other.queue;
      return time < other.time;
    }

    void SetHeapHandle(HeapHandle handle) {
      DCHECK(handle.IsValid());
      queue->set_heap_handle(handle);
    }

    void ClearHeapHandle() {
      DCHECK(queue->heap_handle().IsValid());
      queue->set_heap_handle(HeapHandle());

      DCHECK_NE(queue->scheduled_time_domain_wakeup(), base::TimeTicks());
      queue->set_scheduled_time_domain_wakeup(base::TimeTicks());
    }
  };

  IntrusiveHeap<DelayedWakeup> delayed_wakeup_queue_;

  // This lock guards only |newly_updatable_|.  It's not expected to be heavily
  // contended.
  base::Lock newly_updatable_lock_;
  std::vector<internal::TaskQueueImpl*> newly_updatable_;

  // Set of task queues with avaliable work on the incoming queue.  This should
  // only be accessed from the main thread.
  std::set<internal::TaskQueueImpl*> updatable_queue_set_;

  Observer* observer_;  // NOT OWNED.

  base::ThreadChecker main_thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(TimeDomain);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TIME_DOMAIN_H_
