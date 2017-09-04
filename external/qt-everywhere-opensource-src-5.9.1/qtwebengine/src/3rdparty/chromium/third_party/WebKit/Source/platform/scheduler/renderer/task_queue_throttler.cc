// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/task_queue_throttler.h"

#include <cstdint>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/throttled_time_domain.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "public/platform/WebFrameScheduler.h"

namespace blink {
namespace scheduler {

namespace {

base::Optional<base::TimeTicks> NextTaskRunTime(LazyNow* lazy_now,
                                                TaskQueue* queue) {
  if (queue->HasPendingImmediateWork())
    return lazy_now->Now();
  return queue->GetNextScheduledWakeUp();
}

template <class T>
T Min(const base::Optional<T>& optional, const T& value) {
  if (!optional) {
    return value;
  }
  return std::min(optional.value(), value);
}

template <class T>
base::Optional<T> Min(const base::Optional<T>& a, const base::Optional<T>& b) {
  if (!b)
    return a;
  if (!a)
    return b;
  return std::min(a.value(), b.value());
}

template <class T>
T Max(const base::Optional<T>& optional, const T& value) {
  if (!optional)
    return value;
  return std::max(optional.value(), value);
}

template <class T>
base::Optional<T> Max(const base::Optional<T>& a, const base::Optional<T>& b) {
  if (!b)
    return a;
  if (!a)
    return b;
  return std::max(a.value(), b.value());
}

}  // namespace

TaskQueueThrottler::TimeBudgetPool::TimeBudgetPool(
    const char* name,
    TaskQueueThrottler* task_queue_throttler,
    base::TimeTicks now,
    base::Optional<base::TimeDelta> max_budget_level,
    base::Optional<base::TimeDelta> max_throttling_duration)
    : name_(name),
      task_queue_throttler_(task_queue_throttler),
      max_budget_level_(max_budget_level),
      max_throttling_duration_(max_throttling_duration),
      last_checkpoint_(now),
      cpu_percentage_(1),
      is_enabled_(true) {}

TaskQueueThrottler::TimeBudgetPool::~TimeBudgetPool() {}

void TaskQueueThrottler::TimeBudgetPool::SetTimeBudgetRecoveryRate(
    base::TimeTicks now,
    double cpu_percentage) {
  Advance(now);
  cpu_percentage_ = cpu_percentage;
  EnforceBudgetLevelRestrictions();
}

void TaskQueueThrottler::TimeBudgetPool::AddQueue(base::TimeTicks now,
                                                  TaskQueue* queue) {
  std::pair<TaskQueueMap::iterator, bool> insert_result =
      task_queue_throttler_->queue_details_.insert(
          std::make_pair(queue, Metadata()));
  Metadata& metadata = insert_result.first->second;
  DCHECK(!metadata.time_budget_pool);
  metadata.time_budget_pool = this;

  associated_task_queues_.insert(queue);

  if (!is_enabled_ || !task_queue_throttler_->IsThrottled(queue))
    return;

  queue->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);

  task_queue_throttler_->MaybeSchedulePumpQueue(FROM_HERE, now, queue,
                                                GetNextAllowedRunTime());
}

void TaskQueueThrottler::TimeBudgetPool::RemoveQueue(base::TimeTicks now,
                                                     TaskQueue* queue) {
  auto find_it = task_queue_throttler_->queue_details_.find(queue);
  DCHECK(find_it != task_queue_throttler_->queue_details_.end() &&
         find_it->second.time_budget_pool == this);
  find_it->second.time_budget_pool = nullptr;
  bool is_throttled = task_queue_throttler_->IsThrottled(queue);

  task_queue_throttler_->MaybeDeleteQueueMetadata(find_it);
  associated_task_queues_.erase(queue);

  if (!is_enabled_ || !is_throttled)
    return;

  task_queue_throttler_->MaybeSchedulePumpQueue(FROM_HERE, now, queue,
                                                base::nullopt);
}

void TaskQueueThrottler::TimeBudgetPool::EnableThrottling(LazyNow* lazy_now) {
  if (is_enabled_)
    return;
  is_enabled_ = true;

  BlockThrottledQueues(lazy_now->Now());
}

void TaskQueueThrottler::TimeBudgetPool::DisableThrottling(LazyNow* lazy_now) {
  if (!is_enabled_)
    return;
  is_enabled_ = false;

  for (TaskQueue* queue : associated_task_queues_) {
    if (!task_queue_throttler_->IsThrottled(queue))
      continue;

    task_queue_throttler_->MaybeSchedulePumpQueue(FROM_HERE, lazy_now->Now(),
                                                  queue, base::nullopt);
  }

  // TODO(altimin): We need to disable TimeBudgetQueues here or they will
  // regenerate extra time budget when they are disabled.
}

bool TaskQueueThrottler::TimeBudgetPool::IsThrottlingEnabled() const {
  return is_enabled_;
}

void TaskQueueThrottler::TimeBudgetPool::GrantAdditionalBudget(
    base::TimeTicks now,
    base::TimeDelta budget_level) {
  Advance(now);
  current_budget_level_ += budget_level;
  EnforceBudgetLevelRestrictions();
}

void TaskQueueThrottler::TimeBudgetPool::SetReportingCallback(
    base::Callback<void(base::TimeDelta)> reporting_callback) {
  reporting_callback_ = reporting_callback;
}

void TaskQueueThrottler::TimeBudgetPool::Close() {
  DCHECK_EQ(0u, associated_task_queues_.size());

  task_queue_throttler_->time_budget_pools_.erase(this);
}

bool TaskQueueThrottler::TimeBudgetPool::HasEnoughBudgetToRun(
    base::TimeTicks now) {
  Advance(now);
  return !is_enabled_ || current_budget_level_.InMicroseconds() >= 0;
}

base::TimeTicks TaskQueueThrottler::TimeBudgetPool::GetNextAllowedRunTime() {
  if (!is_enabled_ || current_budget_level_.InMicroseconds() >= 0) {
    return last_checkpoint_;
  } else {
    // Subtract because current_budget is negative.
    return last_checkpoint_ - current_budget_level_ / cpu_percentage_;
  }
}

void TaskQueueThrottler::TimeBudgetPool::RecordTaskRunTime(
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  DCHECK_LE(start_time, end_time);
  Advance(end_time);
  if (is_enabled_) {
    base::TimeDelta old_budget_level = current_budget_level_;
    current_budget_level_ -= (end_time - start_time);
    EnforceBudgetLevelRestrictions();

    if (!reporting_callback_.is_null() && old_budget_level.InSecondsF() > 0 &&
        current_budget_level_.InSecondsF() < 0) {
      reporting_callback_.Run(-current_budget_level_ / cpu_percentage_);
    }
  }
}

const char* TaskQueueThrottler::TimeBudgetPool::Name() const {
  return name_;
}

void TaskQueueThrottler::TimeBudgetPool::AsValueInto(
    base::trace_event::TracedValue* state,
    base::TimeTicks now) const {
  state->BeginDictionary();

  state->SetString("name", name_);
  state->SetDouble("time_budget", cpu_percentage_);
  state->SetDouble("time_budget_level_in_seconds",
                   current_budget_level_.InSecondsF());
  state->SetDouble("last_checkpoint_seconds_ago",
                   (now - last_checkpoint_).InSecondsF());

  state->BeginArray("task_queues");
  for (TaskQueue* queue : associated_task_queues_) {
    state->AppendString(base::StringPrintf(
        "%" PRIx64, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(queue))));
  }
  state->EndArray();

  state->EndDictionary();
}

void TaskQueueThrottler::TimeBudgetPool::Advance(base::TimeTicks now) {
  if (now > last_checkpoint_) {
    if (is_enabled_) {
      current_budget_level_ += cpu_percentage_ * (now - last_checkpoint_);
      EnforceBudgetLevelRestrictions();
    }
    last_checkpoint_ = now;
  }
}

void TaskQueueThrottler::TimeBudgetPool::BlockThrottledQueues(
    base::TimeTicks now) {
  for (TaskQueue* queue : associated_task_queues_) {
    if (!task_queue_throttler_->IsThrottled(queue))
      continue;

    queue->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);
    task_queue_throttler_->MaybeSchedulePumpQueue(FROM_HERE, now, queue,
                                                  base::nullopt);
  }
}

void TaskQueueThrottler::TimeBudgetPool::EnforceBudgetLevelRestrictions() {
  if (max_budget_level_) {
    current_budget_level_ =
        std::min(current_budget_level_, max_budget_level_.value());
  }
  if (max_throttling_duration_) {
    // Current budget level may be negative.
    current_budget_level_ =
        std::max(current_budget_level_,
                 -max_throttling_duration_.value() * cpu_percentage_);
  }
}

TaskQueueThrottler::TaskQueueThrottler(
    RendererSchedulerImpl* renderer_scheduler,
    const char* tracing_category)
    : task_runner_(renderer_scheduler->ControlTaskRunner()),
      renderer_scheduler_(renderer_scheduler),
      tick_clock_(renderer_scheduler->tick_clock()),
      tracing_category_(tracing_category),
      time_domain_(new ThrottledTimeDomain(this, tracing_category)),
      allow_throttling_(true),
      weak_factory_(this) {
  pump_throttled_tasks_closure_.Reset(base::Bind(
      &TaskQueueThrottler::PumpThrottledTasks, weak_factory_.GetWeakPtr()));
  forward_immediate_work_callback_ =
      base::Bind(&TaskQueueThrottler::OnTimeDomainHasImmediateWork,
                 weak_factory_.GetWeakPtr());

  renderer_scheduler_->RegisterTimeDomain(time_domain_.get());
}

TaskQueueThrottler::~TaskQueueThrottler() {
  // It's possible for queues to be still throttled, so we need to tidy up
  // before unregistering the time domain.
  for (const TaskQueueMap::value_type& map_entry : queue_details_) {
    TaskQueue* task_queue = map_entry.first;
    if (IsThrottled(task_queue)) {
      task_queue->SetTimeDomain(renderer_scheduler_->real_time_domain());
      task_queue->RemoveFence();
    }
  }

  renderer_scheduler_->UnregisterTimeDomain(time_domain_.get());
}

void TaskQueueThrottler::IncreaseThrottleRefCount(TaskQueue* task_queue) {
  DCHECK_NE(task_queue, task_runner_.get());

  std::pair<TaskQueueMap::iterator, bool> insert_result =
      queue_details_.insert(std::make_pair(task_queue, Metadata()));
  insert_result.first->second.throttling_ref_count++;

  // If ref_count is 1, the task queue is newly throttled.
  if (insert_result.first->second.throttling_ref_count != 1)
    return;

  TRACE_EVENT1(tracing_category_, "TaskQueueThrottler_TaskQueueThrottled",
               "task_queue", task_queue);

  if (!allow_throttling_)
    return;

  task_queue->SetTimeDomain(time_domain_.get());
  // This blocks any tasks from |task_queue| until PumpThrottledTasks() to
  // enforce task alignment.
  task_queue->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);

  if (!task_queue->IsEmpty()) {
    if (task_queue->HasPendingImmediateWork()) {
      OnTimeDomainHasImmediateWork(task_queue);
    } else {
      OnTimeDomainHasDelayedWork(task_queue);
    }
  }
}

void TaskQueueThrottler::DecreaseThrottleRefCount(TaskQueue* task_queue) {
  TaskQueueMap::iterator iter = queue_details_.find(task_queue);

  if (iter == queue_details_.end() ||
      --iter->second.throttling_ref_count != 0) {
    return;
  }

  TRACE_EVENT1(tracing_category_, "TaskQueueThrottler_TaskQueueUnthrottled",
               "task_queue", task_queue);

  MaybeDeleteQueueMetadata(iter);

  if (!allow_throttling_)
    return;

  task_queue->SetTimeDomain(renderer_scheduler_->real_time_domain());
  task_queue->RemoveFence();
}

bool TaskQueueThrottler::IsThrottled(TaskQueue* task_queue) const {
  if (!allow_throttling_)
    return false;

  auto find_it = queue_details_.find(task_queue);
  if (find_it == queue_details_.end())
    return false;
  return find_it->second.throttling_ref_count > 0;
}

void TaskQueueThrottler::UnregisterTaskQueue(TaskQueue* task_queue) {
  LazyNow lazy_now(tick_clock_);
  auto find_it = queue_details_.find(task_queue);

  if (find_it == queue_details_.end())
    return;

  if (find_it->second.time_budget_pool)
    find_it->second.time_budget_pool->RemoveQueue(lazy_now.Now(), task_queue);

  queue_details_.erase(find_it);
}

void TaskQueueThrottler::OnTimeDomainHasImmediateWork(TaskQueue* queue) {
  // Forward to the main thread if called from another thread
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(forward_immediate_work_callback_, queue));
    return;
  }
  TRACE_EVENT0(tracing_category_,
               "TaskQueueThrottler::OnTimeDomainHasImmediateWork");

  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeTicks next_allowed_run_time = GetNextAllowedRunTime(now, queue);
  MaybeSchedulePumpThrottledTasks(FROM_HERE, now, next_allowed_run_time);
}

void TaskQueueThrottler::OnTimeDomainHasDelayedWork(TaskQueue* queue) {
  TRACE_EVENT0(tracing_category_,
               "TaskQueueThrottler::OnTimeDomainHasDelayedWork");
  base::TimeTicks now = tick_clock_->NowTicks();
  LazyNow lazy_now(now);

  base::Optional<base::TimeTicks> next_scheduled_delayed_task =
      NextTaskRunTime(&lazy_now, queue);
  DCHECK(next_scheduled_delayed_task);
  MaybeSchedulePumpThrottledTasks(FROM_HERE, now,
                                  next_scheduled_delayed_task.value());
}

void TaskQueueThrottler::PumpThrottledTasks() {
  TRACE_EVENT0(tracing_category_, "TaskQueueThrottler::PumpThrottledTasks");
  pending_pump_throttled_tasks_runtime_.reset();

  LazyNow lazy_now(tick_clock_);
  base::Optional<base::TimeTicks> next_scheduled_delayed_task;

  for (const TaskQueueMap::value_type& map_entry : queue_details_) {
    TaskQueue* task_queue = map_entry.first;
    if (task_queue->IsEmpty() || !IsThrottled(task_queue))
      continue;

    // Don't enable queues whose budget pool doesn't allow them to run now.
    base::TimeTicks next_allowed_run_time =
        GetNextAllowedRunTime(lazy_now.Now(), task_queue);
    base::Optional<base::TimeTicks> next_desired_run_time =
        NextTaskRunTime(&lazy_now, task_queue);

    if (next_desired_run_time &&
        next_allowed_run_time > next_desired_run_time.value()) {
      TRACE_EVENT1(
          tracing_category_,
          "TaskQueueThrottler::PumpThrottledTasks_ExpensiveTaskThrottled",
          "throttle_time_in_seconds",
          (next_allowed_run_time - next_desired_run_time.value()).InSecondsF());

      // Schedule a pump for queue which was disabled because of time budget.
      next_scheduled_delayed_task =
          Min(next_scheduled_delayed_task, next_allowed_run_time);

      continue;
    }

    next_scheduled_delayed_task =
        Min(next_scheduled_delayed_task, task_queue->GetNextScheduledWakeUp());

    if (next_allowed_run_time > lazy_now.Now())
      continue;

    // Remove previous fence and install a new one, allowing all tasks posted
    // on |task_queue| up until this point to run and block all further tasks.
    task_queue->InsertFence(TaskQueue::InsertFencePosition::NOW);
  }

  // Maybe schedule a call to TaskQueueThrottler::PumpThrottledTasks if there is
  // a pending delayed task or a throttled task ready to run.
  // NOTE: posting a non-delayed task in the future will result in
  // TaskQueueThrottler::OnTimeDomainHasImmediateWork being called.
  if (next_scheduled_delayed_task) {
    MaybeSchedulePumpThrottledTasks(FROM_HERE, lazy_now.Now(),
                                    *next_scheduled_delayed_task);
  }
}

/* static */
base::TimeTicks TaskQueueThrottler::AlignedThrottledRunTime(
    base::TimeTicks unthrottled_runtime) {
  const base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);
  return unthrottled_runtime + one_second -
         ((unthrottled_runtime - base::TimeTicks()) % one_second);
}

void TaskQueueThrottler::MaybeSchedulePumpThrottledTasks(
    const tracked_objects::Location& from_here,
    base::TimeTicks now,
    base::TimeTicks unaligned_runtime) {
  if (!allow_throttling_)
    return;

  base::TimeTicks runtime =
      std::max(now, AlignedThrottledRunTime(unaligned_runtime));

  // If there is a pending call to PumpThrottledTasks and it's sooner than
  // |runtime| then return.
  if (pending_pump_throttled_tasks_runtime_ &&
      runtime >= pending_pump_throttled_tasks_runtime_.value()) {
    return;
  }

  pending_pump_throttled_tasks_runtime_ = runtime;

  pump_throttled_tasks_closure_.Cancel();

  base::TimeDelta delay = pending_pump_throttled_tasks_runtime_.value() - now;
  TRACE_EVENT1(tracing_category_,
               "TaskQueueThrottler::MaybeSchedulePumpThrottledTasks",
               "delay_till_next_pump_ms", delay.InMilliseconds());
  task_runner_->PostDelayedTask(
      from_here, pump_throttled_tasks_closure_.callback(), delay);
}

TaskQueueThrottler::TimeBudgetPool* TaskQueueThrottler::CreateTimeBudgetPool(
    const char* name,
    base::Optional<base::TimeDelta> max_budget_level,
    base::Optional<base::TimeDelta> max_throttling_duration) {
  TimeBudgetPool* time_budget_pool =
      new TimeBudgetPool(name, this, tick_clock_->NowTicks(), max_budget_level,
                         max_throttling_duration);
  time_budget_pools_[time_budget_pool] = base::WrapUnique(time_budget_pool);
  return time_budget_pool;
}

void TaskQueueThrottler::OnTaskRunTimeReported(TaskQueue* task_queue,
                                               base::TimeTicks start_time,
                                               base::TimeTicks end_time) {
  if (!IsThrottled(task_queue))
    return;

  TimeBudgetPool* time_budget_pool = GetTimeBudgetPoolForQueue(task_queue);
  if (!time_budget_pool)
    return;

  time_budget_pool->RecordTaskRunTime(start_time, end_time);
  if (!time_budget_pool->HasEnoughBudgetToRun(end_time))
    time_budget_pool->BlockThrottledQueues(end_time);
}

void TaskQueueThrottler::AsValueInto(base::trace_event::TracedValue* state,
                                     base::TimeTicks now) const {
  if (pending_pump_throttled_tasks_runtime_) {
    state->SetDouble(
        "next_throttled_tasks_pump_in_seconds",
        (pending_pump_throttled_tasks_runtime_.value() - now).InSecondsF());
  }

  state->BeginDictionary("time_budget_pools");

  for (const auto& map_entry : time_budget_pools_) {
    TaskQueueThrottler::TimeBudgetPool* pool = map_entry.first;
    pool->AsValueInto(state, now);
  }

  state->EndDictionary();
}

TaskQueueThrottler::TimeBudgetPool*
TaskQueueThrottler::GetTimeBudgetPoolForQueue(TaskQueue* queue) {
  auto find_it = queue_details_.find(queue);
  if (find_it == queue_details_.end())
    return nullptr;
  return find_it->second.time_budget_pool;
}

void TaskQueueThrottler::MaybeSchedulePumpQueue(
    const tracked_objects::Location& from_here,
    base::TimeTicks now,
    TaskQueue* queue,
    base::Optional<base::TimeTicks> next_possible_run_time) {
  LazyNow lazy_now(now);
  base::Optional<base::TimeTicks> next_run_time =
      Max(NextTaskRunTime(&lazy_now, queue), next_possible_run_time);

  if (next_run_time) {
    MaybeSchedulePumpThrottledTasks(from_here, now, next_run_time.value());
  }
}

base::TimeTicks TaskQueueThrottler::GetNextAllowedRunTime(base::TimeTicks now,
                                                          TaskQueue* queue) {
  TimeBudgetPool* time_budget_pool = GetTimeBudgetPoolForQueue(queue);
  if (!time_budget_pool)
    return now;
  return std::max(now, time_budget_pool->GetNextAllowedRunTime());
}

void TaskQueueThrottler::MaybeDeleteQueueMetadata(TaskQueueMap::iterator it) {
  if (it->second.throttling_ref_count == 0 && !it->second.time_budget_pool)
    queue_details_.erase(it);
}

void TaskQueueThrottler::DisableThrottling() {
  if (!allow_throttling_)
    return;

  allow_throttling_ = false;

  for (const auto& map_entry : queue_details_) {
    if (map_entry.second.throttling_ref_count == 0)
      continue;

    TaskQueue* queue = map_entry.first;

    queue->SetTimeDomain(renderer_scheduler_->GetActiveTimeDomain());

    queue->RemoveFence();
  }

  pump_throttled_tasks_closure_.Cancel();
  pending_pump_throttled_tasks_runtime_ = base::nullopt;
}

void TaskQueueThrottler::EnableThrottling() {
  if (allow_throttling_)
    return;

  allow_throttling_ = true;

  LazyNow lazy_now(tick_clock_);

  for (const auto& map_entry : queue_details_) {
    if (map_entry.second.throttling_ref_count == 0)
      continue;

    TaskQueue* queue = map_entry.first;

    // Throttling is enabled and task queue should be blocked immediately
    // to enforce task alignment.
    queue->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);
    queue->SetTimeDomain(time_domain_.get());
    MaybeSchedulePumpQueue(FROM_HERE, lazy_now.Now(), queue,
                           GetNextAllowedRunTime(lazy_now.Now(), queue));
  }
}

}  // namespace scheduler
}  // namespace blink
