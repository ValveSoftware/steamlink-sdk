// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_manager.h"

#include <queue>
#include <set>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"
#include "platform/scheduler/base/task_queue_selector.h"
#include "platform/scheduler/base/work_queue.h"
#include "platform/scheduler/base/work_queue_sets.h"
#include "public/platform/scheduler/base/task_time_observer.h"

namespace blink {
namespace scheduler {

namespace {
const size_t kRecordRecordTaskDelayHistogramsEveryNTasks = 10;

void RecordDelayedTaskLateness(base::TimeDelta lateness) {
  UMA_HISTOGRAM_TIMES("RendererScheduler.TaskQueueManager.DelayedTaskLateness",
                      lateness);
}

void RecordImmediateTaskQueueingDuration(tracked_objects::Duration duration) {
  UMA_HISTOGRAM_TIMES(
      "RendererScheduler.TaskQueueManager.ImmediateTaskQueueingDuration",
      base::TimeDelta::FromMilliseconds(duration.InMilliseconds()));
}

double MonotonicTimeInSeconds(base::TimeTicks timeTicks) {
  return (timeTicks - base::TimeTicks()).InSecondsF();
}

// Converts a OnceClosure to a RepeatingClosure. It hits CHECK failure to run
// the resulting RepeatingClosure more than once.
// TODO(tzik): This will be unneeded after the Closure-to-OnceClosure migration
// on TaskRunner finished. Remove it once it gets unneeded.
base::RepeatingClosure UnsafeConvertOnceClosureToRepeating(
    base::OnceClosure cb) {
  return base::BindRepeating([](base::OnceClosure cb) { std::move(cb).Run(); },
                             base::Passed(&cb));
}
}

TaskQueueManager::TaskQueueManager(
    scoped_refptr<TaskQueueManagerDelegate> delegate,
    const char* tracing_category,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : real_time_domain_(new RealTimeDomain(tracing_category)),
      delegate_(delegate),
      task_was_run_on_quiescence_monitored_queue_(false),
      other_thread_pending_wakeup_(false),
      work_batch_size_(1),
      task_count_(0),
      tracing_category_(tracing_category),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
      currently_executing_task_queue_(nullptr),
      observer_(nullptr),
      deletion_sentinel_(new DeletionSentinel()),
      weak_factory_(this) {
  DCHECK(delegate->RunsTasksOnCurrentThread());
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(disabled_by_default_tracing_category,
                                     "TaskQueueManager", this);
  selector_.SetTaskQueueSelectorObserver(this);

  from_main_thread_immediate_do_work_closure_ =
      base::Bind(&TaskQueueManager::DoWork, weak_factory_.GetWeakPtr(),
                 base::TimeTicks(), true);
  from_other_thread_immediate_do_work_closure_ =
      base::Bind(&TaskQueueManager::DoWork, weak_factory_.GetWeakPtr(),
                 base::TimeTicks(), false);

  // TODO(alexclarke): Change this to be a parameter that's passed in.
  RegisterTimeDomain(real_time_domain_.get());

  delegate_->AddNestingObserver(this);
}

TaskQueueManager::~TaskQueueManager() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(disabled_by_default_tracing_category_,
                                     "TaskQueueManager", this);

  while (!queues_.empty())
    (*queues_.begin())->UnregisterTaskQueue();

  selector_.SetTaskQueueSelectorObserver(nullptr);

  delegate_->RemoveNestingObserver(this);
}

void TaskQueueManager::RegisterTimeDomain(TimeDomain* time_domain) {
  time_domains_.insert(time_domain);
  time_domain->OnRegisterWithTaskQueueManager(this);
}

void TaskQueueManager::UnregisterTimeDomain(TimeDomain* time_domain) {
  time_domains_.erase(time_domain);
}

scoped_refptr<internal::TaskQueueImpl> TaskQueueManager::NewTaskQueue(
    const TaskQueue::Spec& spec) {
  TRACE_EVENT1(tracing_category_, "TaskQueueManager::NewTaskQueue",
               "queue_name", TaskQueue::NameForQueueType(spec.type));
  DCHECK(main_thread_checker_.CalledOnValidThread());
  TimeDomain* time_domain =
      spec.time_domain ? spec.time_domain : real_time_domain_.get();
  DCHECK(time_domains_.find(time_domain) != time_domains_.end());
  scoped_refptr<internal::TaskQueueImpl> queue(
      make_scoped_refptr(new internal::TaskQueueImpl(
          this, time_domain, spec, disabled_by_default_tracing_category_,
          disabled_by_default_verbose_tracing_category_)));
  queues_.insert(queue);
  selector_.AddQueue(queue.get());
  return queue;
}

void TaskQueueManager::SetObserver(Observer* observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  observer_ = observer;
}

void TaskQueueManager::UnregisterTaskQueue(
    scoped_refptr<internal::TaskQueueImpl> task_queue) {
  TRACE_EVENT1(tracing_category_, "TaskQueueManager::UnregisterTaskQueue",
               "queue_name", task_queue->GetName());
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (observer_)
    observer_->OnUnregisterTaskQueue(task_queue);

  // Add |task_queue| to |queues_to_delete_| so we can prevent it from being
  // freed while any of our structures hold hold a raw pointer to it.
  queues_to_delete_.insert(task_queue);
  queues_.erase(task_queue);
  selector_.RemoveQueue(task_queue.get());
}

void TaskQueueManager::UpdateWorkQueues(LazyNow lazy_now) {
  TRACE_EVENT0(disabled_by_default_tracing_category_,
               "TaskQueueManager::UpdateWorkQueues");

  for (TimeDomain* time_domain : time_domains_) {
    LazyNow lazy_now_in_domain = time_domain == real_time_domain_.get()
                                     ? lazy_now
                                     : time_domain->CreateLazyNow();
    time_domain->UpdateWorkQueues(lazy_now_in_domain);
  }
}

void TaskQueueManager::OnBeginNestedMessageLoop() {
  // We just entered a nested message loop, make sure there's a DoWork posted or
  // the system will grind to a halt.
  delegate_->PostTask(FROM_HERE, from_main_thread_immediate_do_work_closure_);
}

void TaskQueueManager::MaybeScheduleImmediateWork(
    const tracked_objects::Location& from_here) {
  bool on_main_thread = delegate_->BelongsToCurrentThread();
  // De-duplicate DoWork posts.
  if (on_main_thread) {
    if (!main_thread_pending_wakeups_.insert(base::TimeTicks()).second) {
      return;
    }
    delegate_->PostTask(from_here, from_main_thread_immediate_do_work_closure_);
  } else {
    {
      base::AutoLock lock(other_thread_lock_);
      if (other_thread_pending_wakeup_)
        return;
      other_thread_pending_wakeup_ = true;
    }
    delegate_->PostTask(from_here,
                        from_other_thread_immediate_do_work_closure_);
  }
}

void TaskQueueManager::MaybeScheduleDelayedWork(
    const tracked_objects::Location& from_here,
    base::TimeTicks now,
    base::TimeDelta delay) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_GE(delay, base::TimeDelta());

  // If there's a pending immediate DoWork then we rely on
  // TryAdvanceTimeDomains getting the TimeDomain to call
  // MaybeScheduleDelayedWork again when the immediate DoWork is complete.
  if (main_thread_pending_wakeups_.find(base::TimeTicks()) !=
      main_thread_pending_wakeups_.end()) {
    return;
  }
  // De-duplicate DoWork posts.
  base::TimeTicks run_time = now + delay;
  if (!main_thread_pending_wakeups_.empty() &&
      *main_thread_pending_wakeups_.begin() <= run_time) {
    return;
  }
  main_thread_pending_wakeups_.insert(run_time);
  delegate_->PostDelayedTask(
      from_here, base::Bind(&TaskQueueManager::DoWork,
                            weak_factory_.GetWeakPtr(), run_time, true),
      delay);
}

void TaskQueueManager::DoWork(base::TimeTicks run_time, bool from_main_thread) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  TRACE_EVENT1(tracing_category_, "TaskQueueManager::DoWork",
               "from_main_thread", from_main_thread);

  if (from_main_thread) {
    main_thread_pending_wakeups_.erase(run_time);
  } else {
    base::AutoLock lock(other_thread_lock_);
    other_thread_pending_wakeup_ = false;
  }

  // Posting a DoWork while a DoWork is running leads to spurious DoWorks.
  main_thread_pending_wakeups_.insert(base::TimeTicks());

  if (!delegate_->IsNested())
    queues_to_delete_.clear();

  LazyNow lazy_now(real_time_domain()->CreateLazyNow());
  base::TimeTicks task_start_time;

  if (!delegate_->IsNested() && task_time_observers_.might_have_observers())
    task_start_time = lazy_now.Now();

  UpdateWorkQueues(lazy_now);

  for (int i = 0; i < work_batch_size_; i++) {
    internal::WorkQueue* work_queue;
    if (!SelectWorkQueueToService(&work_queue))
      break;

    // TaskQueueManager guarantees that task queue will not be deleted
    // when we are in DoWork (but WorkQueue may be deleted).
    internal::TaskQueueImpl* task_queue = work_queue->task_queue();

    switch (ProcessTaskFromWorkQueue(work_queue)) {
      case ProcessTaskResult::DEFERRED:
        // If a task was deferred, try again with another task.
        continue;
      case ProcessTaskResult::EXECUTED:
        break;
      case ProcessTaskResult::TASK_QUEUE_MANAGER_DELETED:
        return;  // The TaskQueueManager got deleted, we must bail out.
    }

    lazy_now = real_time_domain()->CreateLazyNow();
    if (!delegate_->IsNested() && task_start_time != base::TimeTicks()) {
      // Only report top level task durations.
      base::TimeTicks task_end_time = lazy_now.Now();
      for (auto& observer : task_time_observers_) {
        observer.ReportTaskTime(task_queue,
                                MonotonicTimeInSeconds(task_start_time),
                                MonotonicTimeInSeconds(task_end_time));
      }
      task_start_time = task_end_time;
    }

    work_queue = nullptr;  // The queue may have been unregistered.

    UpdateWorkQueues(lazy_now);

    // Only run a single task per batch in nested run loops so that we can
    // properly exit the nested loop when someone calls RunLoop::Quit().
    if (delegate_->IsNested())
      break;
  }

  main_thread_pending_wakeups_.erase(base::TimeTicks());

  // TODO(alexclarke): Consider refactoring the above loop to terminate only
  // when there's no more work left to be done, rather than posting a
  // continuation task.
  if (!selector_.EnabledWorkQueuesEmpty() || TryAdvanceTimeDomains())
    MaybeScheduleImmediateWork(FROM_HERE);
}

bool TaskQueueManager::TryAdvanceTimeDomains() {
  bool can_advance = false;
  for (TimeDomain* time_domain : time_domains_) {
    can_advance |= time_domain->MaybeAdvanceTime();
  }
  return can_advance;
}

bool TaskQueueManager::SelectWorkQueueToService(
    internal::WorkQueue** out_work_queue) {
  bool should_run = selector_.SelectWorkQueueToService(out_work_queue);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      disabled_by_default_tracing_category_, "TaskQueueManager", this,
      AsValueWithSelectorResult(should_run, *out_work_queue));
  return should_run;
}

void TaskQueueManager::DidQueueTask(
    const internal::TaskQueueImpl::Task& pending_task) {
  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", pending_task);
}

TaskQueueManager::ProcessTaskResult TaskQueueManager::ProcessTaskFromWorkQueue(
    internal::WorkQueue* work_queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<DeletionSentinel> protect(deletion_sentinel_);
  internal::TaskQueueImpl::Task pending_task =
      work_queue->TakeTaskFromWorkQueue();

  // It's possible the task was canceled, if so bail out.
  if (pending_task.task.IsCancelled())
    return ProcessTaskResult::EXECUTED;

  internal::TaskQueueImpl* queue = work_queue->task_queue();
  if (queue->GetQuiescenceMonitored())
    task_was_run_on_quiescence_monitored_queue_ = true;

  if (!pending_task.nestable && delegate_->IsNested()) {
    // Defer non-nestable work to the main task runner.  NOTE these tasks can be
    // arbitrarily delayed so the additional delay should not be a problem.
    // TODO(skyostil): Figure out a way to not forget which task queue the
    // task is associated with. See http://crbug.com/522843.
    // TODO(tzik): Remove base::UnsafeConvertOnceClosureToRepeating once
    // TaskRunners have migrated to OnceClosure.
    delegate_->PostNonNestableTask(
        pending_task.posted_from,
        UnsafeConvertOnceClosureToRepeating(std::move(pending_task.task)));
    return ProcessTaskResult::DEFERRED;
  }

  MaybeRecordTaskDelayHistograms(pending_task, queue);

  TRACE_TASK_EXECUTION("TaskQueueManager::ProcessTaskFromWorkQueue",
                       pending_task);
  if (queue->GetShouldNotifyObservers()) {
    for (auto& observer : task_observers_)
      observer.WillProcessTask(pending_task);
    queue->NotifyWillProcessTask(pending_task);
  }
  TRACE_EVENT1(tracing_category_, "TaskQueueManager::RunTask", "queue",
               queue->GetName());
  // NOTE when TaskQueues get unregistered a reference ends up getting retained
  // by |queues_to_delete_| which is cleared at the top of |DoWork|. This means
  // we are OK to use raw pointers here.
  internal::TaskQueueImpl* prev_executing_task_queue =
      currently_executing_task_queue_;
  currently_executing_task_queue_ = queue;
  task_annotator_.RunTask("TaskQueueManager::PostTask", &pending_task);
  // Detect if the TaskQueueManager just got deleted.  If this happens we must
  // not access any member variables after this point.
  if (protect->HasOneRef())
    return ProcessTaskResult::TASK_QUEUE_MANAGER_DELETED;

  currently_executing_task_queue_ = prev_executing_task_queue;

  if (queue->GetShouldNotifyObservers()) {
    for (auto& observer : task_observers_)
      observer.DidProcessTask(pending_task);
    queue->NotifyDidProcessTask(pending_task);
  }

  return ProcessTaskResult::EXECUTED;
}

void TaskQueueManager::MaybeRecordTaskDelayHistograms(
    const internal::TaskQueueImpl::Task& pending_task,
    const internal::TaskQueueImpl* queue) {
  if ((task_count_++ % kRecordRecordTaskDelayHistogramsEveryNTasks) != 0)
    return;

  // Record delayed task lateness and immediate task queuing durations.
  if (!pending_task.delayed_run_time.is_null()) {
    RecordDelayedTaskLateness(delegate_->NowTicks() -
                              pending_task.delayed_run_time);
  } else if (!pending_task.time_posted.is_null()) {
    RecordImmediateTaskQueueingDuration(tracked_objects::TrackedTime::Now() -
                                        pending_task.time_posted);
  }
}

bool TaskQueueManager::RunsTasksOnCurrentThread() const {
  return delegate_->RunsTasksOnCurrentThread();
}

void TaskQueueManager::SetWorkBatchSize(int work_batch_size) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_GE(work_batch_size, 1);
  work_batch_size_ = work_batch_size;
}

void TaskQueueManager::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_observers_.AddObserver(task_observer);
}

void TaskQueueManager::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_observers_.RemoveObserver(task_observer);
}

void TaskQueueManager::AddTaskTimeObserver(TaskTimeObserver* task_time_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_time_observers_.AddObserver(task_time_observer);
}

void TaskQueueManager::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_time_observers_.RemoveObserver(task_time_observer);
}

bool TaskQueueManager::GetAndClearSystemIsQuiescentBit() {
  bool task_was_run = task_was_run_on_quiescence_monitored_queue_;
  task_was_run_on_quiescence_monitored_queue_ = false;
  return !task_was_run;
}

const scoped_refptr<TaskQueueManagerDelegate>& TaskQueueManager::delegate()
    const {
  return delegate_;
}

internal::EnqueueOrder TaskQueueManager::GetNextSequenceNumber() {
  return enqueue_order_generator_.GenerateNext();
}

LazyNow TaskQueueManager::CreateLazyNow() const {
  return LazyNow(delegate_.get());
}

size_t TaskQueueManager::GetNumberOfPendingTasks() const {
  size_t task_count = 0;
  for (auto& queue : queues_)
    task_count += queue->GetNumberOfPendingTasks();
  return task_count;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
TaskQueueManager::AsValueWithSelectorResult(
    bool should_run,
    internal::WorkQueue* selected_work_queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->BeginArray("queues");
  for (auto& queue : queues_)
    queue->AsValueInto(state.get());
  state->EndArray();
  state->BeginDictionary("selector");
  selector_.AsValueInto(state.get());
  state->EndDictionary();
  if (should_run) {
    state->SetString("selected_queue",
                     selected_work_queue->task_queue()->GetName());
    state->SetString("work_queue_name", selected_work_queue->name());
  }

  state->BeginArray("time_domains");
  for (auto* time_domain : time_domains_)
    time_domain->AsValueInto(state.get());
  state->EndArray();
  return std::move(state);
}

void TaskQueueManager::OnTaskQueueEnabled(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Only schedule DoWork if there's something to do.
  if (queue->HasPendingImmediateWork())
    MaybeScheduleImmediateWork(FROM_HERE);
}

void TaskQueueManager::OnTriedToSelectBlockedWorkQueue(
    internal::WorkQueue* work_queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!work_queue->Empty());
  if (observer_) {
    observer_->OnTriedToExecuteBlockedTask(*work_queue->task_queue(),
                                           *work_queue->GetFrontTask());
  }
}

bool TaskQueueManager::HasImmediateWorkForTesting() const {
  return !selector_.EnabledWorkQueuesEmpty();
}

}  // namespace scheduler
}  // namespace blink
