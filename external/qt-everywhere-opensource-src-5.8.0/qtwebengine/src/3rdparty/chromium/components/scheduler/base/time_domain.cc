// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/time_domain.h"

#include <set>

#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"
#include "components/scheduler/base/work_queue.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

TimeDomain::TimeDomain(Observer* observer) : observer_(observer) {}

TimeDomain::~TimeDomain() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

void TimeDomain::RegisterQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
}

void TimeDomain::UnregisterQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
  UnregisterAsUpdatableTaskQueue(queue);

  // We need to remove |task_queue| from delayed_wakeup_multimap_ which is a
  // little awkward since it's keyed by time. O(n) running time.
  for (DelayedWakeupMultimap::iterator iter = delayed_wakeup_multimap_.begin();
       iter != delayed_wakeup_multimap_.end();) {
    if (iter->second == queue) {
      // O(1) amortized.
      iter = delayed_wakeup_multimap_.erase(iter);
    } else {
      iter++;
    }
  }
}

void TimeDomain::MigrateQueue(internal::TaskQueueImpl* queue,
                              TimeDomain* destination_time_domain) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(queue->GetTimeDomain(), this);
  DCHECK(destination_time_domain);
  UnregisterAsUpdatableTaskQueue(queue);

  base::TimeTicks destination_now = destination_time_domain->Now();
  // We need to remove |task_queue| from delayed_wakeup_multimap_ which is a
  // little awkward since it's keyed by time. O(n) running time.
  for (DelayedWakeupMultimap::iterator iter = delayed_wakeup_multimap_.begin();
       iter != delayed_wakeup_multimap_.end();) {
    if (iter->second == queue) {
      destination_time_domain->ScheduleDelayedWork(queue, iter->first,
                                                   destination_now);
      // O(1) amortized.
      iter = delayed_wakeup_multimap_.erase(iter);
    } else {
      iter++;
    }
  }
}

void TimeDomain::ScheduleDelayedWork(internal::TaskQueueImpl* queue,
                                     base::TimeTicks delayed_run_time,
                                     base::TimeTicks now) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_multimap_.empty() ||
      delayed_run_time < delayed_wakeup_multimap_.begin()->first) {
    base::TimeDelta delay = std::max(base::TimeDelta(), delayed_run_time - now);
    RequestWakeup(now, delay);
  }

  delayed_wakeup_multimap_.insert(std::make_pair(delayed_run_time, queue));
  if (observer_)
    observer_->OnTimeDomainHasDelayedWork();
}

void TimeDomain::RegisterAsUpdatableTaskQueue(internal::TaskQueueImpl* queue) {
  {
    base::AutoLock lock(newly_updatable_lock_);
    newly_updatable_.push_back(queue);
  }
  if (observer_)
    observer_->OnTimeDomainHasImmediateWork();
}

void TimeDomain::UnregisterAsUpdatableTaskQueue(
    internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  updatable_queue_set_.erase(queue);

  base::AutoLock lock(newly_updatable_lock_);
  // Remove all copies of |queue| from |newly_updatable_|.
  for (size_t i = 0; i < newly_updatable_.size();) {
    if (newly_updatable_[i] == queue) {
      // Move last element into slot #i and then compact.
      newly_updatable_[i] = newly_updatable_.back();
      newly_updatable_.pop_back();
    } else {
      i++;
    }
  }
}

void TimeDomain::UpdateWorkQueues(
    bool should_trigger_wakeup,
    const internal::TaskQueueImpl::Task* previous_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  LazyNow lazy_now(CreateLazyNow());

  // Move any ready delayed tasks into the Incoming queues.
  WakeupReadyDelayedQueues(&lazy_now, should_trigger_wakeup, previous_task);

  MoveNewlyUpdatableQueuesIntoUpdatableQueueSet();

  auto iter = updatable_queue_set_.begin();
  while (iter != updatable_queue_set_.end()) {
    internal::TaskQueueImpl* queue = *iter++;
    // NOTE Update work queue may erase itself from |updatable_queue_set_|.
    // This is fine, erasing an element won't invalidate any interator, as long
    // as the iterator isn't the element being delated.
    if (queue->immediate_work_queue()->Empty())
      queue->UpdateImmediateWorkQueue(should_trigger_wakeup, previous_task);
  }
}

void TimeDomain::MoveNewlyUpdatableQueuesIntoUpdatableQueueSet() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(newly_updatable_lock_);
  while (!newly_updatable_.empty()) {
    updatable_queue_set_.insert(newly_updatable_.back());
    newly_updatable_.pop_back();
  }
}

void TimeDomain::WakeupReadyDelayedQueues(
    LazyNow* lazy_now,
    bool should_trigger_wakeup,
    const internal::TaskQueueImpl::Task* previous_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Wake up any queues with pending delayed work.  Note std::multipmap stores
  // the elements sorted by key, so the begin() iterator points to the earliest
  // queue to wakeup.
  std::set<internal::TaskQueueImpl*> dedup_set;
  while (!delayed_wakeup_multimap_.empty()) {
    DelayedWakeupMultimap::iterator next_wakeup =
        delayed_wakeup_multimap_.begin();
    if (next_wakeup->first > lazy_now->Now())
      break;
    // A queue could have any number of delayed tasks pending so it's worthwhile
    // deduping calls to UpdateDelayedWorkQueue since it takes a lock.
    // NOTE the order in which these are called matters since the order
    // in which EnqueueTaskLocks is called is respected when choosing which
    // queue to execute a task from.
    if (dedup_set.insert(next_wakeup->second).second) {
      next_wakeup->second->UpdateDelayedWorkQueue(
          lazy_now, should_trigger_wakeup, previous_task);
    }
    delayed_wakeup_multimap_.erase(next_wakeup);
  }
}

void TimeDomain::ClearExpiredWakeups() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  LazyNow lazy_now(CreateLazyNow());
  while (!delayed_wakeup_multimap_.empty()) {
    DelayedWakeupMultimap::iterator next_wakeup =
        delayed_wakeup_multimap_.begin();
    if (next_wakeup->first > lazy_now.Now())
      break;
    delayed_wakeup_multimap_.erase(next_wakeup);
  }
}

bool TimeDomain::NextScheduledRunTime(base::TimeTicks* out_time) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_multimap_.empty())
    return false;

  *out_time = delayed_wakeup_multimap_.begin()->first;
  return true;
}

bool TimeDomain::NextScheduledTaskQueue(TaskQueue** out_task_queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (delayed_wakeup_multimap_.empty())
    return false;

  *out_task_queue = delayed_wakeup_multimap_.begin()->second;
  return true;
}

void TimeDomain::AsValueInto(base::trace_event::TracedValue* state) const {
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->BeginArray("updatable_queue_set");
  for (auto& queue : updatable_queue_set_)
    state->AppendString(queue->GetName());
  state->EndArray();
  state->SetInteger("registered_delay_count", delayed_wakeup_multimap_.size());
  if (!delayed_wakeup_multimap_.empty()) {
    base::TimeDelta delay = delayed_wakeup_multimap_.begin()->first - Now();
    state->SetDouble("next_delay_ms", delay.InMillisecondsF());
  }
  AsValueIntoInternal(state);
  state->EndDictionary();
}

}  // namespace scheduler
