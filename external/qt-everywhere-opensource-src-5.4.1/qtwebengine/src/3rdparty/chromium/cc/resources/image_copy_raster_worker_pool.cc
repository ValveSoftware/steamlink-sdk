// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/image_copy_raster_worker_pool.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/scoped_resource.h"

namespace cc {

// static
scoped_ptr<RasterWorkerPool> ImageCopyRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool) {
  return make_scoped_ptr<RasterWorkerPool>(new ImageCopyRasterWorkerPool(
      task_runner, task_graph_runner, resource_provider, resource_pool));
}

ImageCopyRasterWorkerPool::ImageCopyRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      resource_provider_(resource_provider),
      resource_pool_(resource_pool),
      has_performed_copy_since_last_flush_(false),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false),
      raster_finished_weak_ptr_factory_(this) {}

ImageCopyRasterWorkerPool::~ImageCopyRasterWorkerPool() {
  DCHECK_EQ(0u, raster_task_states_.size());
}

Rasterizer* ImageCopyRasterWorkerPool::AsRasterizer() { return this; }

void ImageCopyRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void ImageCopyRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "ImageCopyRasterWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void ImageCopyRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "ImageCopyRasterWorkerPool::ScheduleTasks");

  DCHECK_EQ(queue->required_for_activation_count,
            static_cast<size_t>(
                std::count_if(queue->items.begin(),
                              queue->items.end(),
                              RasterTaskQueue::Item::IsRequiredForActivation)));

  if (!raster_tasks_pending_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  raster_tasks_pending_ = true;
  raster_tasks_required_for_activation_pending_ = true;

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<RasterizerTask>
      new_raster_required_for_activation_finished_task(
          CreateRasterRequiredForActivationFinishedTask(
              queue->required_for_activation_count,
              task_runner_.get(),
              base::Bind(&ImageCopyRasterWorkerPool::
                             OnRasterRequiredForActivationFinished,
                         raster_finished_weak_ptr_factory_.GetWeakPtr())));
  scoped_refptr<RasterizerTask> new_raster_finished_task(
      CreateRasterFinishedTask(
          task_runner_.get(),
          base::Bind(&ImageCopyRasterWorkerPool::OnRasterFinished,
                     raster_finished_weak_ptr_factory_.GetWeakPtr())));

  resource_pool_->CheckBusyResources();

  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    if (item.required_for_activation) {
      graph_.edges.push_back(TaskGraph::Edge(
          task, new_raster_required_for_activation_finished_task.get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);

    graph_.edges.push_back(
        TaskGraph::Edge(task, new_raster_finished_task.get()));
  }

  InsertNodeForTask(&graph_,
                    new_raster_required_for_activation_finished_task.get(),
                    kRasterRequiredForActivationFinishedTaskPriority,
                    queue->required_for_activation_count);
  InsertNodeForTask(&graph_,
                    new_raster_finished_task.get(),
                    kRasterFinishedTaskPriority,
                    queue->items.size());

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  raster_finished_task_ = new_raster_finished_task;
  raster_required_for_activation_finished_task_ =
      new_raster_required_for_activation_finished_task;

  resource_pool_->ReduceResourceUsage();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      "rasterizing",
      "state",
      TracedValue::FromValue(StateAsValue().release()));
}

void ImageCopyRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "ImageCopyRasterWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    RasterizerTask* task = static_cast<RasterizerTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();

  FlushCopies();
}

SkCanvas* ImageCopyRasterWorkerPool::AcquireCanvasForRaster(RasterTask* task) {
  DCHECK_EQ(task->resource()->format(), resource_pool_->resource_format());
  scoped_ptr<ScopedResource> resource(
      resource_pool_->AcquireResource(task->resource()->size()));
  SkCanvas* canvas = resource_provider_->MapImageRasterBuffer(resource->id());
  DCHECK(std::find_if(raster_task_states_.begin(),
                      raster_task_states_.end(),
                      RasterTaskState::TaskComparator(task)) ==
         raster_task_states_.end());
  raster_task_states_.push_back(RasterTaskState(task, resource.release()));
  return canvas;
}

void ImageCopyRasterWorkerPool::ReleaseCanvasForRaster(RasterTask* task) {
  RasterTaskState::Vector::iterator it =
      std::find_if(raster_task_states_.begin(),
                   raster_task_states_.end(),
                   RasterTaskState::TaskComparator(task));
  DCHECK(it != raster_task_states_.end());
  scoped_ptr<ScopedResource> resource(it->resource);
  std::swap(*it, raster_task_states_.back());
  raster_task_states_.pop_back();

  bool content_has_changed =
      resource_provider_->UnmapImageRasterBuffer(resource->id());

  // |content_has_changed| can be false as result of task being canceled or
  // task implementation deciding not to modify bitmap (ie. analysis of raster
  // commands detected content as a solid color).
  if (content_has_changed) {
    resource_provider_->CopyResource(resource->id(), task->resource()->id());
    has_performed_copy_since_last_flush_ = true;
  }

  resource_pool_->ReleaseResource(resource.Pass());
}

void ImageCopyRasterWorkerPool::OnRasterFinished() {
  TRACE_EVENT0("cc", "ImageCopyRasterWorkerPool::OnRasterFinished");

  DCHECK(raster_tasks_pending_);
  raster_tasks_pending_ = false;
  TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  client_->DidFinishRunningTasks();
}

void ImageCopyRasterWorkerPool::OnRasterRequiredForActivationFinished() {
  TRACE_EVENT0(
      "cc", "ImageCopyRasterWorkerPool::OnRasterRequiredForActivationFinished");

  DCHECK(raster_tasks_required_for_activation_pending_);
  raster_tasks_required_for_activation_pending_ = false;
  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      "rasterizing",
      "state",
      TracedValue::FromValue(StateAsValue().release()));
  client_->DidFinishRunningTasksRequiredForActivation();
}

void ImageCopyRasterWorkerPool::FlushCopies() {
  if (!has_performed_copy_since_last_flush_)
    return;

  resource_provider_->ShallowFlushIfSupported();
  has_performed_copy_since_last_flush_ = false;
}

scoped_ptr<base::Value> ImageCopyRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetInteger("pending_count", raster_task_states_.size());
  state->SetBoolean("tasks_required_for_activation_pending",
                    raster_tasks_required_for_activation_pending_);
  state->Set("staging_state", StagingStateAsValue().release());

  return state.PassAs<base::Value>();
}
scoped_ptr<base::Value> ImageCopyRasterWorkerPool::StagingStateAsValue() const {
  scoped_ptr<base::DictionaryValue> staging_state(new base::DictionaryValue);

  staging_state->SetInteger("staging_resource_count",
                            resource_pool_->total_resource_count());
  staging_state->SetInteger("bytes_used_for_staging_resources",
                            resource_pool_->total_memory_usage_bytes());
  staging_state->SetInteger("pending_copy_count",
                            resource_pool_->total_resource_count() -
                                resource_pool_->acquired_resource_count());
  staging_state->SetInteger("bytes_pending_copy",
                            resource_pool_->total_memory_usage_bytes() -
                                resource_pool_->acquired_memory_usage_bytes());

  return staging_state.PassAs<base::Value>();
}

}  // namespace cc
