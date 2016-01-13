// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/pixel_buffer_raster_worker_pool.h"

#include <algorithm>

#include "base/containers/stack_container.h"
#include "base/debug/trace_event.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource.h"

namespace cc {
namespace {

const int kCheckForCompletedRasterTasksDelayMs = 6;

const size_t kMaxScheduledRasterTasks = 48;

typedef base::StackVector<RasterTask*, kMaxScheduledRasterTasks>
    RasterTaskVector;

}  // namespace

// static
scoped_ptr<RasterWorkerPool> PixelBufferRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    size_t max_transfer_buffer_usage_bytes) {
  return make_scoped_ptr<RasterWorkerPool>(
      new PixelBufferRasterWorkerPool(task_runner,
                                      task_graph_runner,
                                      resource_provider,
                                      max_transfer_buffer_usage_bytes));
}

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    size_t max_transfer_buffer_usage_bytes)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      resource_provider_(resource_provider),
      shutdown_(false),
      scheduled_raster_task_count_(0u),
      raster_tasks_required_for_activation_count_(0u),
      bytes_pending_upload_(0u),
      max_bytes_pending_upload_(max_transfer_buffer_usage_bytes),
      has_performed_uploads_since_last_flush_(false),
      should_notify_client_if_no_tasks_are_pending_(false),
      should_notify_client_if_no_tasks_required_for_activation_are_pending_(
          false),
      raster_finished_task_pending_(false),
      raster_required_for_activation_finished_task_pending_(false),
      check_for_completed_raster_task_notifier_(
          task_runner,
          base::Bind(&PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(
              kCheckForCompletedRasterTasksDelayMs)),
      raster_finished_weak_ptr_factory_(this) {
}

PixelBufferRasterWorkerPool::~PixelBufferRasterWorkerPool() {
  DCHECK_EQ(0u, raster_task_states_.size());
  DCHECK_EQ(0u, raster_tasks_with_pending_upload_.size());
  DCHECK_EQ(0u, completed_raster_tasks_.size());
  DCHECK_EQ(0u, completed_image_decode_tasks_.size());
  DCHECK_EQ(0u, raster_tasks_required_for_activation_count_);
}

Rasterizer* PixelBufferRasterWorkerPool::AsRasterizer() { return this; }

void PixelBufferRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void PixelBufferRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::Shutdown");

  shutdown_ = true;

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);

  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();

  check_for_completed_raster_task_notifier_.Cancel();

  for (RasterTaskState::Vector::iterator it = raster_task_states_.begin();
       it != raster_task_states_.end();
       ++it) {
    RasterTaskState& state = *it;

    // All unscheduled tasks need to be canceled.
    if (state.type == RasterTaskState::UNSCHEDULED) {
      completed_raster_tasks_.push_back(state.task);
      state.type = RasterTaskState::COMPLETED;
    }
  }
  DCHECK_EQ(completed_raster_tasks_.size(), raster_task_states_.size());
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  DCHECK_EQ(queue->required_for_activation_count,
            static_cast<size_t>(
                std::count_if(queue->items.begin(),
                              queue->items.end(),
                              RasterTaskQueue::Item::IsRequiredForActivation)));

  if (!should_notify_client_if_no_tasks_are_pending_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  should_notify_client_if_no_tasks_are_pending_ = true;
  should_notify_client_if_no_tasks_required_for_activation_are_pending_ = true;

  raster_tasks_required_for_activation_count_ = 0u;

  // Update raster task state and remove items from old queue.
  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;

    // Remove any old items that are associated with this task. The result is
    // that the old queue is left with all items not present in this queue,
    // which we use below to determine what tasks need to be canceled.
    RasterTaskQueue::Item::Vector::iterator old_it =
        std::find_if(raster_tasks_.items.begin(),
                     raster_tasks_.items.end(),
                     RasterTaskQueue::Item::TaskComparator(task));
    if (old_it != raster_tasks_.items.end()) {
      std::swap(*old_it, raster_tasks_.items.back());
      raster_tasks_.items.pop_back();
    }

    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    if (state_it != raster_task_states_.end()) {
      RasterTaskState& state = *state_it;

      state.required_for_activation = item.required_for_activation;
      // |raster_tasks_required_for_activation_count| accounts for all tasks
      // that need to complete before we can send a "ready to activate" signal.
      // Tasks that have already completed should not be part of this count.
      if (state.type != RasterTaskState::COMPLETED) {
        raster_tasks_required_for_activation_count_ +=
            item.required_for_activation;
      }
      continue;
    }

    DCHECK(!task->HasBeenScheduled());
    raster_task_states_.push_back(
        RasterTaskState(task, item.required_for_activation));
    raster_tasks_required_for_activation_count_ += item.required_for_activation;
  }

  // Determine what tasks in old queue need to be canceled.
  for (RasterTaskQueue::Item::Vector::const_iterator it =
           raster_tasks_.items.begin();
       it != raster_tasks_.items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;

    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    // We've already processed completion if we can't find a RasterTaskState for
    // this task.
    if (state_it == raster_task_states_.end())
      continue;

    RasterTaskState& state = *state_it;

    // Unscheduled task can be canceled.
    if (state.type == RasterTaskState::UNSCHEDULED) {
      DCHECK(!task->HasBeenScheduled());
      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) == completed_raster_tasks_.end());
      completed_raster_tasks_.push_back(task);
      state.type = RasterTaskState::COMPLETED;
    }

    // No longer required for activation.
    state.required_for_activation = false;
  }

  raster_tasks_.Swap(queue);

  // Check for completed tasks when ScheduleTasks() is called as
  // priorities might have changed and this maximizes the number
  // of top priority tasks that are scheduled.
  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Schedule new tasks.
  ScheduleMoreTasks();

  // Reschedule check for completed raster tasks.
  check_for_completed_raster_task_notifier_.Schedule();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      StateName(),
      "state",
      TracedValue::FromValue(StateAsValue().release()));
}

void PixelBufferRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::CheckForCompletedTasks");

  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();
  FlushUploads();

  for (RasterizerTask::Vector::const_iterator it =
           completed_image_decode_tasks_.begin();
       it != completed_image_decode_tasks_.end();
       ++it) {
    RasterizerTask* task = it->get();
    task->RunReplyOnOriginThread();
  }
  completed_image_decode_tasks_.clear();

  for (RasterTask::Vector::const_iterator it = completed_raster_tasks_.begin();
       it != completed_raster_tasks_.end();
       ++it) {
    RasterTask* task = it->get();
    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    DCHECK(state_it != raster_task_states_.end());
    DCHECK_EQ(RasterTaskState::COMPLETED, state_it->type);

    std::swap(*state_it, raster_task_states_.back());
    raster_task_states_.pop_back();

    task->RunReplyOnOriginThread();
  }
  completed_raster_tasks_.clear();
}

SkCanvas* PixelBufferRasterWorkerPool::AcquireCanvasForRaster(
    RasterTask* task) {
  DCHECK(std::find_if(raster_task_states_.begin(),
                      raster_task_states_.end(),
                      RasterTaskState::TaskComparator(task)) !=
         raster_task_states_.end());
  resource_provider_->AcquirePixelRasterBuffer(task->resource()->id());
  return resource_provider_->MapPixelRasterBuffer(task->resource()->id());
}

void PixelBufferRasterWorkerPool::ReleaseCanvasForRaster(RasterTask* task) {
  DCHECK(std::find_if(raster_task_states_.begin(),
                      raster_task_states_.end(),
                      RasterTaskState::TaskComparator(task)) !=
         raster_task_states_.end());
  resource_provider_->ReleasePixelRasterBuffer(task->resource()->id());
}

void PixelBufferRasterWorkerPool::OnRasterFinished() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::OnRasterFinished");

  // |should_notify_client_if_no_tasks_are_pending_| can be set to false as
  // a result of a scheduled CheckForCompletedRasterTasks() call. No need to
  // perform another check in that case as we've already notified the client.
  if (!should_notify_client_if_no_tasks_are_pending_)
    return;
  raster_finished_task_pending_ = false;

  // Call CheckForCompletedRasterTasks() when we've finished running all
  // raster tasks needed since last time ScheduleTasks() was called.
  // This reduces latency between the time when all tasks have finished
  // running and the time when the client is notified.
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::OnRasterRequiredForActivationFinished() {
  TRACE_EVENT0(
      "cc",
      "PixelBufferRasterWorkerPool::OnRasterRequiredForActivationFinished");

  // Analogous to OnRasterTasksFinished(), there's no need to call
  // CheckForCompletedRasterTasks() if the client has already been notified.
  if (!should_notify_client_if_no_tasks_required_for_activation_are_pending_)
    return;
  raster_required_for_activation_finished_task_pending_ = false;

  // This reduces latency between the time when all tasks required for
  // activation have finished running and the time when the client is
  // notified.
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::FlushUploads() {
  if (!has_performed_uploads_since_last_flush_)
    return;

  resource_provider_->ShallowFlushIfSupported();
  has_performed_uploads_since_last_flush_ = false;
}

void PixelBufferRasterWorkerPool::CheckForCompletedUploads() {
  RasterTask::Vector tasks_with_completed_uploads;

  // First check if any have completed.
  while (!raster_tasks_with_pending_upload_.empty()) {
    RasterTask* task = raster_tasks_with_pending_upload_.front().get();
    DCHECK(std::find_if(raster_task_states_.begin(),
                        raster_task_states_.end(),
                        RasterTaskState::TaskComparator(task)) !=
           raster_task_states_.end());
    DCHECK_EQ(RasterTaskState::UPLOADING,
              std::find_if(raster_task_states_.begin(),
                           raster_task_states_.end(),
                           RasterTaskState::TaskComparator(task))->type);

    // Uploads complete in the order they are issued.
    if (!resource_provider_->DidSetPixelsComplete(task->resource()->id()))
      break;

    tasks_with_completed_uploads.push_back(task);
    raster_tasks_with_pending_upload_.pop_front();
  }

  DCHECK(client_);
  bool should_force_some_uploads_to_complete =
      shutdown_ || client_->ShouldForceTasksRequiredForActivationToComplete();

  if (should_force_some_uploads_to_complete) {
    RasterTask::Vector tasks_with_uploads_to_force;
    RasterTaskDeque::iterator it = raster_tasks_with_pending_upload_.begin();
    while (it != raster_tasks_with_pending_upload_.end()) {
      RasterTask* task = it->get();
      RasterTaskState::Vector::const_iterator state_it =
          std::find_if(raster_task_states_.begin(),
                       raster_task_states_.end(),
                       RasterTaskState::TaskComparator(task));
      DCHECK(state_it != raster_task_states_.end());
      const RasterTaskState& state = *state_it;

      // Force all uploads required for activation to complete.
      // During shutdown, force all pending uploads to complete.
      if (shutdown_ || state.required_for_activation) {
        tasks_with_uploads_to_force.push_back(task);
        tasks_with_completed_uploads.push_back(task);
        it = raster_tasks_with_pending_upload_.erase(it);
        continue;
      }

      ++it;
    }

    // Force uploads in reverse order. Since forcing can cause a wait on
    // all previous uploads, we would rather wait only once downstream.
    for (RasterTask::Vector::reverse_iterator it =
             tasks_with_uploads_to_force.rbegin();
         it != tasks_with_uploads_to_force.rend();
         ++it) {
      RasterTask* task = it->get();

      resource_provider_->ForceSetPixelsToComplete(task->resource()->id());
      has_performed_uploads_since_last_flush_ = true;
    }
  }

  // Release shared memory and move tasks with completed uploads
  // to |completed_raster_tasks_|.
  for (RasterTask::Vector::const_iterator it =
           tasks_with_completed_uploads.begin();
       it != tasks_with_completed_uploads.end();
       ++it) {
    RasterTask* task = it->get();
    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    DCHECK(state_it != raster_task_states_.end());
    RasterTaskState& state = *state_it;

    bytes_pending_upload_ -= task->resource()->bytes();

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    // Async set pixels commands are not necessarily processed in-sequence with
    // drawing commands. Read lock fences are required to ensure that async
    // commands don't access the resource while used for drawing.
    resource_provider_->EnableReadLockFences(task->resource()->id(), true);

    DCHECK(std::find(completed_raster_tasks_.begin(),
                     completed_raster_tasks_.end(),
                     task) == completed_raster_tasks_.end());
    completed_raster_tasks_.push_back(task);
    state.type = RasterTaskState::COMPLETED;
    DCHECK_LE(static_cast<size_t>(state.required_for_activation),
              raster_tasks_required_for_activation_count_);
    raster_tasks_required_for_activation_count_ -=
        state.required_for_activation;
  }
}

void PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks() {
  TRACE_EVENT0("cc",
               "PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks");

  // Since this function can be called directly, cancel any pending checks.
  check_for_completed_raster_task_notifier_.Cancel();

  DCHECK(should_notify_client_if_no_tasks_are_pending_);

  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Determine what client notifications to generate.
  bool will_notify_client_that_no_tasks_required_for_activation_are_pending =
      (should_notify_client_if_no_tasks_required_for_activation_are_pending_ &&
       !raster_required_for_activation_finished_task_pending_ &&
       !HasPendingTasksRequiredForActivation());
  bool will_notify_client_that_no_tasks_are_pending =
      (should_notify_client_if_no_tasks_are_pending_ &&
       !raster_required_for_activation_finished_task_pending_ &&
       !raster_finished_task_pending_ && !HasPendingTasks());

  // Adjust the need to generate notifications before scheduling more tasks.
  should_notify_client_if_no_tasks_required_for_activation_are_pending_ &=
      !will_notify_client_that_no_tasks_required_for_activation_are_pending;
  should_notify_client_if_no_tasks_are_pending_ &=
      !will_notify_client_that_no_tasks_are_pending;

  scheduled_raster_task_count_ = 0;
  if (PendingRasterTaskCount())
    ScheduleMoreTasks();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      StateName(),
      "state",
      TracedValue::FromValue(StateAsValue().release()));

  // Schedule another check for completed raster tasks while there are
  // pending raster tasks or pending uploads.
  if (HasPendingTasks())
    check_for_completed_raster_task_notifier_.Schedule();

  // Generate client notifications.
  if (will_notify_client_that_no_tasks_required_for_activation_are_pending) {
    DCHECK(!HasPendingTasksRequiredForActivation());
    client_->DidFinishRunningTasksRequiredForActivation();
  }
  if (will_notify_client_that_no_tasks_are_pending) {
    TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
    DCHECK(!HasPendingTasksRequiredForActivation());
    client_->DidFinishRunningTasks();
  }
}

void PixelBufferRasterWorkerPool::ScheduleMoreTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleMoreTasks");

  RasterTaskVector tasks;
  RasterTaskVector tasks_required_for_activation;

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  size_t bytes_pending_upload = bytes_pending_upload_;
  bool did_throttle_raster_tasks = false;
  bool did_throttle_raster_tasks_required_for_activation = false;

  for (RasterTaskQueue::Item::Vector::const_iterator it =
           raster_tasks_.items.begin();
       it != raster_tasks_.items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;

    // |raster_task_states_| contains the state of all tasks that we have not
    // yet run reply callbacks for.
    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    if (state_it == raster_task_states_.end())
      continue;

    RasterTaskState& state = *state_it;

    // Skip task if completed.
    if (state.type == RasterTaskState::COMPLETED) {
      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) != completed_raster_tasks_.end());
      continue;
    }

    // All raster tasks need to be throttled by bytes of pending uploads.
    size_t new_bytes_pending_upload = bytes_pending_upload;
    new_bytes_pending_upload += task->resource()->bytes();
    if (new_bytes_pending_upload > max_bytes_pending_upload_) {
      did_throttle_raster_tasks = true;
      if (item.required_for_activation)
        did_throttle_raster_tasks_required_for_activation = true;
      continue;
    }

    // If raster has finished, just update |bytes_pending_upload|.
    if (state.type == RasterTaskState::UPLOADING) {
      DCHECK(!task->HasCompleted());
      bytes_pending_upload = new_bytes_pending_upload;
      continue;
    }

    // Throttle raster tasks based on kMaxScheduledRasterTasks.
    if (tasks.container().size() >= kMaxScheduledRasterTasks) {
      did_throttle_raster_tasks = true;
      if (item.required_for_activation)
        did_throttle_raster_tasks_required_for_activation = true;
      continue;
    }

    // Update |bytes_pending_upload| now that task has cleared all
    // throttling limits.
    bytes_pending_upload = new_bytes_pending_upload;

    DCHECK(state.type == RasterTaskState::UNSCHEDULED ||
           state.type == RasterTaskState::SCHEDULED);
    state.type = RasterTaskState::SCHEDULED;

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);

    tasks.container().push_back(task);
    if (item.required_for_activation)
      tasks_required_for_activation.container().push_back(task);
  }

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<RasterizerTask>
      new_raster_required_for_activation_finished_task;

  size_t scheduled_raster_task_required_for_activation_count =
      tasks_required_for_activation.container().size();
  DCHECK_LE(scheduled_raster_task_required_for_activation_count,
            raster_tasks_required_for_activation_count_);
  // Schedule OnRasterTasksRequiredForActivationFinished call only when
  // notification is pending and throttling is not preventing all pending
  // tasks required for activation from being scheduled.
  if (!did_throttle_raster_tasks_required_for_activation &&
      should_notify_client_if_no_tasks_required_for_activation_are_pending_) {
    new_raster_required_for_activation_finished_task =
        CreateRasterRequiredForActivationFinishedTask(
            raster_tasks_.required_for_activation_count,
            task_runner_.get(),
            base::Bind(&PixelBufferRasterWorkerPool::
                           OnRasterRequiredForActivationFinished,
                       raster_finished_weak_ptr_factory_.GetWeakPtr()));
    raster_required_for_activation_finished_task_pending_ = true;
    InsertNodeForTask(&graph_,
                      new_raster_required_for_activation_finished_task.get(),
                      kRasterRequiredForActivationFinishedTaskPriority,
                      scheduled_raster_task_required_for_activation_count);
    for (RasterTaskVector::ContainerType::const_iterator it =
             tasks_required_for_activation.container().begin();
         it != tasks_required_for_activation.container().end();
         ++it) {
      graph_.edges.push_back(TaskGraph::Edge(
          *it, new_raster_required_for_activation_finished_task.get()));
    }
  }

  scoped_refptr<RasterizerTask> new_raster_finished_task;

  size_t scheduled_raster_task_count = tasks.container().size();
  DCHECK_LE(scheduled_raster_task_count, PendingRasterTaskCount());
  // Schedule OnRasterTasksFinished call only when notification is pending
  // and throttling is not preventing all pending tasks from being scheduled.
  if (!did_throttle_raster_tasks &&
      should_notify_client_if_no_tasks_are_pending_) {
    new_raster_finished_task = CreateRasterFinishedTask(
        task_runner_.get(),
        base::Bind(&PixelBufferRasterWorkerPool::OnRasterFinished,
                   raster_finished_weak_ptr_factory_.GetWeakPtr()));
    raster_finished_task_pending_ = true;
    InsertNodeForTask(&graph_,
                      new_raster_finished_task.get(),
                      kRasterFinishedTaskPriority,
                      scheduled_raster_task_count);
    for (RasterTaskVector::ContainerType::const_iterator it =
             tasks.container().begin();
         it != tasks.container().end();
         ++it) {
      graph_.edges.push_back(
          TaskGraph::Edge(*it, new_raster_finished_task.get()));
    }
  }

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  scheduled_raster_task_count_ = scheduled_raster_task_count;

  raster_finished_task_ = new_raster_finished_task;
  raster_required_for_activation_finished_task_ =
      new_raster_required_for_activation_finished_task;
}

unsigned PixelBufferRasterWorkerPool::PendingRasterTaskCount() const {
  unsigned num_completed_raster_tasks =
      raster_tasks_with_pending_upload_.size() + completed_raster_tasks_.size();
  DCHECK_GE(raster_task_states_.size(), num_completed_raster_tasks);
  return raster_task_states_.size() - num_completed_raster_tasks;
}

bool PixelBufferRasterWorkerPool::HasPendingTasks() const {
  return PendingRasterTaskCount() || !raster_tasks_with_pending_upload_.empty();
}

bool PixelBufferRasterWorkerPool::HasPendingTasksRequiredForActivation() const {
  return !!raster_tasks_required_for_activation_count_;
}

const char* PixelBufferRasterWorkerPool::StateName() const {
  if (scheduled_raster_task_count_)
    return "rasterizing";
  if (PendingRasterTaskCount())
    return "throttled";
  if (!raster_tasks_with_pending_upload_.empty())
    return "waiting_for_uploads";

  return "finishing";
}

void PixelBufferRasterWorkerPool::CheckForCompletedRasterizerTasks() {
  TRACE_EVENT0("cc",
               "PixelBufferRasterWorkerPool::CheckForCompletedRasterizerTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    RasterizerTask* task = static_cast<RasterizerTask*>(it->get());

    RasterTask* raster_task = task->AsRasterTask();
    if (!raster_task) {
      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      completed_image_decode_tasks_.push_back(task);
      continue;
    }

    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(raster_task));
    DCHECK(state_it != raster_task_states_.end());

    RasterTaskState& state = *state_it;
    DCHECK_EQ(RasterTaskState::SCHEDULED, state.type);

    // Balanced with MapPixelRasterBuffer() call in AcquireCanvasForRaster().
    bool content_has_changed = resource_provider_->UnmapPixelRasterBuffer(
        raster_task->resource()->id());

    // |content_has_changed| can be false as result of task being canceled or
    // task implementation deciding not to modify bitmap (ie. analysis of raster
    // commands detected content as a solid color).
    if (!content_has_changed) {
      raster_task->WillComplete();
      raster_task->CompleteOnOriginThread(this);
      raster_task->DidComplete();

      if (!raster_task->HasFinishedRunning()) {
        // When priorites change, a raster task can be canceled as a result of
        // no longer being of high enough priority to fit in our throttled
        // raster task budget. The task has not yet completed in this case.
        RasterTaskQueue::Item::Vector::const_iterator item_it =
            std::find_if(raster_tasks_.items.begin(),
                         raster_tasks_.items.end(),
                         RasterTaskQueue::Item::TaskComparator(raster_task));
        if (item_it != raster_tasks_.items.end()) {
          state.type = RasterTaskState::UNSCHEDULED;
          continue;
        }
      }

      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       raster_task) == completed_raster_tasks_.end());
      completed_raster_tasks_.push_back(raster_task);
      state.type = RasterTaskState::COMPLETED;
      DCHECK_LE(static_cast<size_t>(state.required_for_activation),
                raster_tasks_required_for_activation_count_);
      raster_tasks_required_for_activation_count_ -=
          state.required_for_activation;
      continue;
    }

    DCHECK(raster_task->HasFinishedRunning());

    resource_provider_->BeginSetPixels(raster_task->resource()->id());
    has_performed_uploads_since_last_flush_ = true;

    bytes_pending_upload_ += raster_task->resource()->bytes();
    raster_tasks_with_pending_upload_.push_back(raster_task);
    state.type = RasterTaskState::UPLOADING;
  }
  completed_tasks_.clear();
}

scoped_ptr<base::Value> PixelBufferRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetInteger("completed_count", completed_raster_tasks_.size());
  state->SetInteger("pending_count", raster_task_states_.size());
  state->SetInteger("pending_upload_count",
                    raster_tasks_with_pending_upload_.size());
  state->SetInteger("pending_required_for_activation_count",
                    raster_tasks_required_for_activation_count_);
  state->Set("throttle_state", ThrottleStateAsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> PixelBufferRasterWorkerPool::ThrottleStateAsValue()
    const {
  scoped_ptr<base::DictionaryValue> throttle_state(new base::DictionaryValue);

  throttle_state->SetInteger("bytes_available_for_upload",
                             max_bytes_pending_upload_ - bytes_pending_upload_);
  throttle_state->SetInteger("bytes_pending_upload", bytes_pending_upload_);
  throttle_state->SetInteger("scheduled_raster_task_count",
                             scheduled_raster_task_count_);
  return throttle_state.PassAs<base::Value>();
}

}  // namespace cc
