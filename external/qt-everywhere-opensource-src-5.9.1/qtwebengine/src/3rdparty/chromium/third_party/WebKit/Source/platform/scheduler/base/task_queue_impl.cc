// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_impl.h"

#include "base/trace_event/blame_context.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"
#include "platform/scheduler/base/time_domain.h"
#include "platform/scheduler/base/work_queue.h"

namespace blink {
namespace scheduler {

// static
const char* TaskQueue::NameForQueueType(TaskQueue::QueueType queue_type) {
  switch (queue_type) {
    case TaskQueue::QueueType::CONTROL:
      return "control_tq";
    case TaskQueue::QueueType::DEFAULT:
      return "default_tq";
    case TaskQueue::QueueType::DEFAULT_LOADING:
      return "default_loading_tq";
    case TaskQueue::QueueType::DEFAULT_TIMER:
      return "default_timer_tq";
    case TaskQueue::QueueType::UNTHROTTLED:
      return "unthrottled_tq";
    case TaskQueue::QueueType::FRAME_LOADING:
      return "frame_loading_tq";
    case TaskQueue::QueueType::FRAME_TIMER:
      return "frame_timer_tq";
    case TaskQueue::QueueType::FRAME_UNTHROTTLED:
      return "frame_unthrottled_tq";
    case TaskQueue::QueueType::COMPOSITOR:
      return "compositor_tq";
    case TaskQueue::QueueType::IDLE:
      return "idle_tq";
    case TaskQueue::QueueType::TEST:
      return "test_tq";
    case TaskQueue::QueueType::COUNT:
      DCHECK(false);
      return nullptr;
  }
  DCHECK(false);
  return nullptr;
}

namespace internal {

TaskQueueImpl::TaskQueueImpl(
    TaskQueueManager* task_queue_manager,
    TimeDomain* time_domain,
    const Spec& spec,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : thread_id_(base::PlatformThread::CurrentId()),
      any_thread_(task_queue_manager, time_domain),
      type_(spec.type),
      name_(NameForQueueType(spec.type)),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
      main_thread_only_(task_queue_manager, this, time_domain),
      should_monitor_quiescence_(spec.should_monitor_quiescence),
      should_notify_observers_(spec.should_notify_observers),
      should_report_when_execution_blocked_(
          spec.should_report_when_execution_blocked) {
  DCHECK(time_domain);
  time_domain->RegisterQueue(this);
}

TaskQueueImpl::~TaskQueueImpl() {
#if DCHECK_IS_ON()
  base::AutoLock lock(any_thread_lock_);
  // NOTE this check shouldn't fire because |TaskQueueManager::queues_|
  // contains a strong reference to this TaskQueueImpl and the TaskQueueManager
  // destructor calls UnregisterTaskQueue on all task queues.
  DCHECK(any_thread().task_queue_manager == nullptr)
      << "UnregisterTaskQueue must be called first!";

#endif
}

TaskQueueImpl::Task::Task()
    : PendingTask(tracked_objects::Location(),
                  base::Closure(),
                  base::TimeTicks(),
                  true),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = 0;
}

TaskQueueImpl::Task::Task(const tracked_objects::Location& posted_from,
                          const base::Closure& task,
                          base::TimeTicks desired_run_time,
                          EnqueueOrder sequence_number,
                          bool nestable)
    : PendingTask(posted_from, task, desired_run_time, nestable),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = sequence_number;
}

TaskQueueImpl::Task::Task(const tracked_objects::Location& posted_from,
                          const base::Closure& task,
                          base::TimeTicks desired_run_time,
                          EnqueueOrder sequence_number,
                          bool nestable,
                          EnqueueOrder enqueue_order)
    : PendingTask(posted_from, task, desired_run_time, nestable),
#ifndef NDEBUG
      enqueue_order_set_(true),
#endif
      enqueue_order_(enqueue_order) {
  sequence_num = sequence_number;
}

TaskQueueImpl::AnyThread::AnyThread(TaskQueueManager* task_queue_manager,
                                    TimeDomain* time_domain)
    : task_queue_manager(task_queue_manager), time_domain(time_domain) {}

TaskQueueImpl::AnyThread::~AnyThread() {}

TaskQueueImpl::MainThreadOnly::MainThreadOnly(
    TaskQueueManager* task_queue_manager,
    TaskQueueImpl* task_queue,
    TimeDomain* time_domain)
    : task_queue_manager(task_queue_manager),
      time_domain(time_domain),
      delayed_work_queue(new WorkQueue(task_queue, "delayed")),
      immediate_work_queue(new WorkQueue(task_queue, "immediate")),
      set_index(0),
      is_enabled(true),
      blame_context(nullptr),
      current_fence(0) {}

TaskQueueImpl::MainThreadOnly::~MainThreadOnly() {}

void TaskQueueImpl::UnregisterTaskQueue() {
  base::AutoLock lock(any_thread_lock_);
  if (main_thread_only().time_domain)
    main_thread_only().time_domain->UnregisterQueue(this);
  if (!any_thread().task_queue_manager)
    return;
  any_thread().time_domain = nullptr;
  main_thread_only().time_domain = nullptr;
  any_thread().task_queue_manager->UnregisterTaskQueue(this);

  any_thread().task_queue_manager = nullptr;
  main_thread_only().task_queue_manager = nullptr;
  main_thread_only().delayed_incoming_queue = std::priority_queue<Task>();
  any_thread().immediate_incoming_queue = std::queue<Task>();
  main_thread_only().immediate_work_queue.reset();
  main_thread_only().delayed_work_queue.reset();
}

bool TaskQueueImpl::RunsTasksOnCurrentThread() const {
  base::AutoLock lock(any_thread_lock_);
  return base::PlatformThread::CurrentId() == thread_id_;
}


bool TaskQueueImpl::PostDelayedTask(const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  if (delay.is_zero())
    return PostImmediateTaskImpl(from_here, task, TaskType::NORMAL);

  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NORMAL);
}

bool TaskQueueImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  if (delay.is_zero())
    return PostImmediateTaskImpl(from_here, task, TaskType::NON_NESTABLE);

  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NON_NESTABLE);
}

bool TaskQueueImpl::PostImmediateTaskImpl(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    TaskType task_type) {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return false;

  EnqueueOrder sequence_number =
      any_thread().task_queue_manager->GetNextSequenceNumber();

  PushOntoImmediateIncomingQueueLocked(
      from_here,
      task,
      base::TimeTicks(),
      sequence_number,
      task_type != TaskType::NON_NESTABLE);
  return true;
}

bool TaskQueueImpl::PostDelayedTaskImpl(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    TaskType task_type) {
  DCHECK_GT(delay, base::TimeDelta());
  if (base::PlatformThread::CurrentId() == thread_id_) {
    // Lock-free fast path for delayed tasks posted from the main thread.
    if (!main_thread_only().task_queue_manager)
      return false;

    EnqueueOrder sequence_number =
        main_thread_only().task_queue_manager->GetNextSequenceNumber();

    base::TimeTicks time_domain_now = main_thread_only().time_domain->Now();
    base::TimeTicks time_domain_delayed_run_time = time_domain_now + delay;
    PushOntoDelayedIncomingQueueFromMainThread(
        Task(from_here, task, time_domain_delayed_run_time, sequence_number,
             task_type != TaskType::NON_NESTABLE),
        time_domain_now);
  } else {
    // NOTE posting a delayed task from a different thread is not expected to
    // be common. This pathway is less optimal than perhaps it could be
    // because it causes two main thread tasks to be run.  Should this
    // assumption prove to be false in future, we may need to revisit this.
    base::AutoLock lock(any_thread_lock_);
    if (!any_thread().task_queue_manager)
      return false;

    EnqueueOrder sequence_number =
        any_thread().task_queue_manager->GetNextSequenceNumber();

    base::TimeTicks time_domain_now = any_thread().time_domain->Now();
    base::TimeTicks time_domain_delayed_run_time = time_domain_now + delay;
    PushOntoDelayedIncomingQueueLocked(
        Task(from_here, task, time_domain_delayed_run_time, sequence_number,
             task_type != TaskType::NON_NESTABLE));
  }
  return true;
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueFromMainThread(
    Task pending_task, base::TimeTicks now) {
  base::TimeTicks delayed_run_time = pending_task.delayed_run_time;
  main_thread_only().task_queue_manager->DidQueueTask(pending_task);
  main_thread_only().delayed_incoming_queue.push(std::move(pending_task));

  // If |pending_task| is at the head of the queue, then make sure a wakeup
  // is requested.
  if (main_thread_only().delayed_incoming_queue.top().delayed_run_time ==
      delayed_run_time) {
    main_thread_only().time_domain->ScheduleDelayedWork(
        this, pending_task.delayed_run_time, now);
  }

  TraceQueueSize(false);
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueLocked(Task pending_task) {
  any_thread().task_queue_manager->DidQueueTask(pending_task);

  int thread_hop_task_sequence_number =
      any_thread().task_queue_manager->GetNextSequenceNumber();
  PushOntoImmediateIncomingQueueLocked(
     FROM_HERE,
     base::Bind(&TaskQueueImpl::ScheduleDelayedWorkTask, this,
                base::Passed(&pending_task)),
     base::TimeTicks(),
     thread_hop_task_sequence_number,
     false);
}

void TaskQueueImpl::ScheduleDelayedWorkTask(Task pending_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::TimeTicks delayed_run_time = pending_task.delayed_run_time;
  base::TimeTicks time_domain_now = main_thread_only().time_domain->Now();
  if (delayed_run_time <= time_domain_now) {
    // If |delayed_run_time| is in the past then push it onto the work queue
    // immediately. To ensure the right task ordering we need to temporarily
    // push it onto the |delayed_incoming_queue|.
    delayed_run_time = time_domain_now;
    pending_task.delayed_run_time = time_domain_now;
    main_thread_only().delayed_incoming_queue.push(std::move(pending_task));
    LazyNow lazy_now(time_domain_now);
    WakeUpForDelayedWork(&lazy_now);
  } else {
    // If |delayed_run_time| is in the future we can queue it as normal.
    PushOntoDelayedIncomingQueueFromMainThread(std::move(pending_task),
                                               time_domain_now);
  }
  TraceQueueSize(false);
}

void TaskQueueImpl::PushOntoImmediateIncomingQueueLocked(
    const tracked_objects::Location& posted_from,
    const base::Closure& task,
    base::TimeTicks desired_run_time,
    EnqueueOrder sequence_number,
    bool nestable) {
  if (any_thread().immediate_incoming_queue.empty())
    any_thread().time_domain->RegisterAsUpdatableTaskQueue(this);
  // If the |immediate_incoming_queue| is empty we need a DoWork posted to make
  // it run.
  if (any_thread().immediate_incoming_queue.empty()) {
    // There's no point posting a DoWork for a disabled queue, however we can
    // only tell if it's disabled from the main thread.
    if (base::PlatformThread::CurrentId() == thread_id_) {
      if (main_thread_only().is_enabled && !BlockedByFenceLocked())
        any_thread().task_queue_manager->MaybeScheduleImmediateWork(FROM_HERE);
    } else {
      any_thread().task_queue_manager->MaybeScheduleImmediateWork(FROM_HERE);
    }
  }
  any_thread().immediate_incoming_queue.emplace(
      posted_from, task, desired_run_time, sequence_number, nestable, sequence_number);
  any_thread().task_queue_manager->DidQueueTask( any_thread().immediate_incoming_queue.back());
  TraceQueueSize(true);
}

void TaskQueueImpl::SetQueueEnabled(bool enabled) {
  if (main_thread_only().is_enabled == enabled)
    return;
  main_thread_only().is_enabled = enabled;
  if (!main_thread_only().task_queue_manager)
    return;
  if (enabled) {
    // Note it's the job of the selector to tell the TaskQueueManager if
    // a DoWork needs posting.
    main_thread_only().task_queue_manager->selector_.EnableQueue(this);
  } else {
    main_thread_only().task_queue_manager->selector_.DisableQueue(this);
  }
}

bool TaskQueueImpl::IsQueueEnabled() const {
  return main_thread_only().is_enabled;
}

bool TaskQueueImpl::IsEmpty() const {
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().delayed_incoming_queue.empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return false;
  }

  base::AutoLock lock(any_thread_lock_);
  return any_thread().immediate_incoming_queue.empty();
}

size_t TaskQueueImpl::GetNumberOfPendingTasks() const {
  size_t task_count = 0;
  task_count += main_thread_only().delayed_work_queue->Size();
  task_count += main_thread_only().delayed_incoming_queue.size();
  task_count += main_thread_only().immediate_work_queue->Size();

  base::AutoLock lock(any_thread_lock_);
  task_count += any_thread().immediate_incoming_queue.size();
  return task_count;
}

bool TaskQueueImpl::HasPendingImmediateWork() const {
  // Any work queue tasks count as immediate work.
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  // Tasks on |delayed_incoming_queue| that could run now, count as
  // immediate work.
  if (!main_thread_only().delayed_incoming_queue.empty() &&
      main_thread_only().delayed_incoming_queue.top().delayed_run_time <=
          main_thread_only().time_domain->CreateLazyNow().Now()) {
    return true;
  }

  // Finally tasks on |immediate_incoming_queue| count as immediate work.
  base::AutoLock lock(any_thread_lock_);
  return !any_thread().immediate_incoming_queue.empty();
}

base::Optional<base::TimeTicks> TaskQueueImpl::GetNextScheduledWakeUp() {
  if (main_thread_only().delayed_incoming_queue.empty())
    return base::nullopt;

  return main_thread_only().delayed_incoming_queue.top().delayed_run_time;
}

void TaskQueueImpl::WakeUpForDelayedWork(LazyNow* lazy_now) {
  // Enqueue all delayed tasks that should be running now, skipping any that
  // have been canceled.
  while (!main_thread_only().delayed_incoming_queue.empty()) {
    Task& task =
        const_cast<Task&>(main_thread_only().delayed_incoming_queue.top());
    if (task.task.IsCancelled()) {
      main_thread_only().delayed_incoming_queue.pop();
      continue;
    }
    if (task.delayed_run_time > lazy_now->Now())
      break;
    task.set_enqueue_order(
        main_thread_only().task_queue_manager->GetNextSequenceNumber());
    main_thread_only().delayed_work_queue->Push(std::move(task));
    main_thread_only().delayed_incoming_queue.pop();
  }

  // Make sure the next wake up is scheduled.
  if (!main_thread_only().delayed_incoming_queue.empty()) {
    main_thread_only().time_domain->ScheduleDelayedWork(
        this, main_thread_only().delayed_incoming_queue.top().delayed_run_time,
        lazy_now->Now());
  }
}

bool TaskQueueImpl::MaybeUpdateImmediateWorkQueues() {
  if (!main_thread_only().task_queue_manager)
    return false;

  if (!main_thread_only().immediate_work_queue->Empty())
    return true;

  base::AutoLock lock(any_thread_lock_);
  main_thread_only().immediate_work_queue->SwapLocked(
      any_thread().immediate_incoming_queue);
  // |immediate_work_queue| is now empty so updates are no longer required.
  return false;
}

void TaskQueueImpl::TraceQueueSize(bool is_locked) const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(disabled_by_default_tracing_category_,
                                     &is_tracing);
  if (!is_tracing)
    return;

  // It's only safe to access the work queues from the main thread.
  // TODO(alexclarke): We should find another way of tracing this
  if (base::PlatformThread::CurrentId() != thread_id_)
    return;

  if (!is_locked)
    any_thread_lock_.Acquire();
  else
    any_thread_lock_.AssertAcquired();
  TRACE_COUNTER1(disabled_by_default_tracing_category_, GetName(),
                 any_thread().immediate_incoming_queue.size() +
                     main_thread_only().immediate_work_queue->Size() +
                     main_thread_only().delayed_work_queue->Size() +
                     main_thread_only().delayed_incoming_queue.size());
  if (!is_locked)
    any_thread_lock_.Release();
}

const char* TaskQueueImpl::GetName() const {
  return name_;
}

TaskQueue::QueueType TaskQueueImpl::GetQueueType() const {
  return type_;
}

void TaskQueueImpl::SetQueuePriority(QueuePriority priority) {
  if (!main_thread_only().task_queue_manager || priority == GetQueuePriority())
    return;
  main_thread_only().task_queue_manager->selector_.SetQueuePriority(this,
                                                                    priority);
}

TaskQueueImpl::QueuePriority TaskQueueImpl::GetQueuePriority() const {
  size_t set_index = immediate_work_queue()->work_queue_set_index();
  DCHECK_EQ(set_index, delayed_work_queue()->work_queue_set_index());
  return static_cast<TaskQueue::QueuePriority>(set_index);
}

// static
const char* TaskQueueImpl::PriorityToString(QueuePriority priority) {
  switch (priority) {
    case CONTROL_PRIORITY:
      return "control";
    case HIGH_PRIORITY:
      return "high";
    case NORMAL_PRIORITY:
      return "normal";
    case BEST_EFFORT_PRIORITY:
      return "best_effort";
    default:
      NOTREACHED();
      return nullptr;
  }
}

void TaskQueueImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  base::AutoLock lock(any_thread_lock_);
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->SetBoolean("enabled", main_thread_only().is_enabled);
  state->SetString("time_domain_name",
                   main_thread_only().time_domain->GetName());
  bool verbose_tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      disabled_by_default_verbose_tracing_category_, &verbose_tracing_enabled);
  state->SetInteger("immediate_incoming_queue_size",
                    any_thread().immediate_incoming_queue.size());
  state->SetInteger("delayed_incoming_queue_size",
                    main_thread_only().delayed_incoming_queue.size());
  state->SetInteger("immediate_work_queue_size",
                    main_thread_only().immediate_work_queue->Size());
  state->SetInteger("delayed_work_queue_size",
                    main_thread_only().delayed_work_queue->Size());
  if (!main_thread_only().delayed_incoming_queue.empty()) {
    base::TimeDelta delay_to_next_task =
        (main_thread_only().delayed_incoming_queue.top().delayed_run_time -
         main_thread_only().time_domain->CreateLazyNow().Now());
    state->SetDouble("delay_to_next_task_ms",
                     delay_to_next_task.InMillisecondsF());
  }
  if (main_thread_only().current_fence)
    state->SetInteger("current_fence", main_thread_only().current_fence);
  if (verbose_tracing_enabled) {
    state->BeginArray("immediate_incoming_queue");
    QueueAsValueInto(any_thread().immediate_incoming_queue, state);
    state->EndArray();
    state->BeginArray("delayed_work_queue");
    main_thread_only().delayed_work_queue->AsValueInto(state);
    state->EndArray();
    state->BeginArray("immediate_work_queue");
    main_thread_only().immediate_work_queue->AsValueInto(state);
    state->EndArray();
    state->BeginArray("delayed_incoming_queue");
    QueueAsValueInto(main_thread_only().delayed_incoming_queue, state);
    state->EndArray();
  }
  state->SetString("priority", PriorityToString(GetQueuePriority()));
  state->EndDictionary();
}

void TaskQueueImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.AddObserver(task_observer);
}

void TaskQueueImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.RemoveObserver(task_observer);
}

void TaskQueueImpl::NotifyWillProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  if (main_thread_only().blame_context)
    main_thread_only().blame_context->Enter();
  for (auto& observer : main_thread_only().task_observers)
    observer.WillProcessTask(pending_task);
}

void TaskQueueImpl::NotifyDidProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  for (auto& observer : main_thread_only().task_observers)
    observer.DidProcessTask(pending_task);
  if (main_thread_only().blame_context)
    main_thread_only().blame_context->Leave();
}

void TaskQueueImpl::SetTimeDomain(TimeDomain* time_domain) {
  {
    base::AutoLock lock(any_thread_lock_);
    DCHECK(time_domain);
    // NOTE this is similar to checking |any_thread().task_queue_manager| but
    // the TaskQueueSelectorTests constructs TaskQueueImpl directly with a null
    // task_queue_manager.  Instead we check |any_thread().time_domain| which is
    // another way of asserting that UnregisterTaskQueue has not been called.
    DCHECK(any_thread().time_domain);
    if (!any_thread().time_domain)
      return;
    DCHECK(main_thread_checker_.CalledOnValidThread());
    if (time_domain == main_thread_only().time_domain)
      return;

    any_thread().time_domain = time_domain;
  }
  // We rely here on TimeDomain::MigrateQueue being thread-safe to use with
  // TimeDomain::Register/UnregisterAsUpdatableTaskQueue.
  main_thread_only().time_domain->MigrateQueue(this, time_domain);
  main_thread_only().time_domain = time_domain;
}

TimeDomain* TaskQueueImpl::GetTimeDomain() const {
  if (base::PlatformThread::CurrentId() == thread_id_)
    return main_thread_only().time_domain;

  base::AutoLock lock(any_thread_lock_);
  return any_thread().time_domain;
}

void TaskQueueImpl::SetBlameContext(
    base::trace_event::BlameContext* blame_context) {
  main_thread_only().blame_context = blame_context;
}

void TaskQueueImpl::InsertFence(TaskQueue::InsertFencePosition position) {
  if (!main_thread_only().task_queue_manager)
    return;

  EnqueueOrder previous_fence = main_thread_only().current_fence;
  main_thread_only().current_fence =
      position == TaskQueue::InsertFencePosition::NOW
          ? main_thread_only().task_queue_manager->GetNextSequenceNumber()
          : static_cast<EnqueueOrder>(EnqueueOrderValues::BLOCKING_FENCE);

  // Tasks posted after this point will have a strictly higher enqueue order
  // and will be blocked from running.
  bool task_unblocked = main_thread_only().immediate_work_queue->InsertFence(
      main_thread_only().current_fence);
  task_unblocked |= main_thread_only().delayed_work_queue->InsertFence(
      main_thread_only().current_fence);

  if (!task_unblocked && previous_fence &&
      previous_fence < main_thread_only().current_fence) {
    base::AutoLock lock(any_thread_lock_);
    if (!any_thread().immediate_incoming_queue.empty() &&
        any_thread().immediate_incoming_queue.front().enqueue_order() >
            previous_fence &&
        any_thread().immediate_incoming_queue.front().enqueue_order() <
            main_thread_only().current_fence) {
      task_unblocked = true;
    }
  }

  if (main_thread_only().is_enabled && task_unblocked) {
    main_thread_only().task_queue_manager->MaybeScheduleImmediateWork(
        FROM_HERE);
  }
}

void TaskQueueImpl::RemoveFence() {
  if (!main_thread_only().task_queue_manager)
    return;

  EnqueueOrder previous_fence = main_thread_only().current_fence;
  main_thread_only().current_fence = 0;

  bool task_unblocked = main_thread_only().immediate_work_queue->RemoveFence();
  task_unblocked |= main_thread_only().delayed_work_queue->RemoveFence();

  if (!task_unblocked && previous_fence) {
    base::AutoLock lock(any_thread_lock_);
    if (!any_thread().immediate_incoming_queue.empty() &&
        any_thread().immediate_incoming_queue.front().enqueue_order() >
            previous_fence) {
      task_unblocked = true;
    }
  }

  if (main_thread_only().is_enabled && task_unblocked) {
    main_thread_only().task_queue_manager->MaybeScheduleImmediateWork(
        FROM_HERE);
  }
}

bool TaskQueueImpl::BlockedByFence() const {
  if (!main_thread_only().current_fence)
    return false;

  if (!main_thread_only().immediate_work_queue->BlockedByFence() ||
      !main_thread_only().delayed_work_queue->BlockedByFence()) {
    return false;
  }

  base::AutoLock lock(any_thread_lock_);
  if (any_thread().immediate_incoming_queue.empty())
    return true;

  return any_thread().immediate_incoming_queue.front().enqueue_order() >
         main_thread_only().current_fence;
}

bool TaskQueueImpl::BlockedByFenceLocked() const {
  if (!main_thread_only().current_fence)
    return false;

  if (!main_thread_only().immediate_work_queue->BlockedByFence() ||
      !main_thread_only().delayed_work_queue->BlockedByFence()) {
    return false;
  }

  if (any_thread().immediate_incoming_queue.empty())
    return true;

  return any_thread().immediate_incoming_queue.front().enqueue_order() >
         main_thread_only().current_fence;
}

EnqueueOrder TaskQueueImpl::GetFenceForTest() const {
  return main_thread_only().current_fence;
}

// static
void TaskQueueImpl::QueueAsValueInto(const std::queue<Task>& queue,
                                     base::trace_event::TracedValue* state) {
  // Remove const to search |queue| in the destructive manner. Restore the
  // content from |visited| later.
  std::queue<Task>* mutable_queue = const_cast<std::queue<Task>*>(&queue);
  std::queue<Task> visited;
  while (!mutable_queue->empty()) {
    TaskAsValueInto(mutable_queue->front(), state);
    visited.push(std::move(mutable_queue->front()));
    mutable_queue->pop();
  }
  *mutable_queue = std::move(visited);
}

// static
void TaskQueueImpl::QueueAsValueInto(const std::priority_queue<Task>& queue,
                                     base::trace_event::TracedValue* state) {
  // Remove const to search |queue| in the destructive manner. Restore the
  // content from |visited| later.
  std::priority_queue<Task>* mutable_queue =
      const_cast<std::priority_queue<Task>*>(&queue);
  std::priority_queue<Task> visited;
  while (!mutable_queue->empty()) {
    TaskAsValueInto(mutable_queue->top(), state);
    visited.push(std::move(const_cast<Task&>(mutable_queue->top())));
    mutable_queue->pop();
  }
  *mutable_queue = std::move(visited);
}

// static
void TaskQueueImpl::TaskAsValueInto(const Task& task,
                                    base::trace_event::TracedValue* state) {
  state->BeginDictionary();
  state->SetString("posted_from", task.posted_from.ToString());
#ifndef NDEBUG
  if (task.enqueue_order_set())
    state->SetInteger("enqueue_order", task.enqueue_order());
#else
  state->SetInteger("enqueue_order", task.enqueue_order());
#endif
  state->SetInteger("sequence_num", task.sequence_num);
  state->SetBoolean("nestable", task.nestable);
  state->SetBoolean("is_high_res", task.is_high_res);
  state->SetBoolean("is_cancelled", task.task.IsCancelled());
  state->SetDouble(
      "delayed_run_time",
      (task.delayed_run_time - base::TimeTicks()).InMicroseconds() / 1000.0L);
  state->EndDictionary();
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
