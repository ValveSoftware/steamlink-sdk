// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/direct_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "cc/output/context_provider.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {

// static
scoped_ptr<RasterWorkerPool> DirectRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    ResourceProvider* resource_provider,
    ContextProvider* context_provider) {
  return make_scoped_ptr<RasterWorkerPool>(new DirectRasterWorkerPool(
      task_runner, resource_provider, context_provider));
}

DirectRasterWorkerPool::DirectRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    ResourceProvider* resource_provider,
    ContextProvider* context_provider)
    : task_runner_(task_runner),
      task_graph_runner_(new TaskGraphRunner),
      namespace_token_(task_graph_runner_->GetNamespaceToken()),
      resource_provider_(resource_provider),
      context_provider_(context_provider),
      run_tasks_on_origin_thread_pending_(false),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false),
      raster_finished_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {}

DirectRasterWorkerPool::~DirectRasterWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

Rasterizer* DirectRasterWorkerPool::AsRasterizer() { return this; }

void DirectRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void DirectRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void DirectRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::ScheduleTasks");

  DCHECK_EQ(queue->required_for_activation_count,
            static_cast<size_t>(
                std::count_if(queue->items.begin(),
                              queue->items.end(),
                              RasterTaskQueue::Item::IsRequiredForActivation)));

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
              base::Bind(&DirectRasterWorkerPool::
                             OnRasterRequiredForActivationFinished,
                         raster_finished_weak_ptr_factory_.GetWeakPtr())));
  scoped_refptr<RasterizerTask> new_raster_finished_task(
      CreateRasterFinishedTask(
          task_runner_.get(),
          base::Bind(&DirectRasterWorkerPool::OnRasterFinished,
                     raster_finished_weak_ptr_factory_.GetWeakPtr())));

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

  ScheduleRunTasksOnOriginThread();

  raster_finished_task_ = new_raster_finished_task;
  raster_required_for_activation_finished_task_ =
      new_raster_required_for_activation_finished_task;
}

void DirectRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::CheckForCompletedTasks");

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
}

SkCanvas* DirectRasterWorkerPool::AcquireCanvasForRaster(RasterTask* task) {
  return resource_provider_->MapDirectRasterBuffer(task->resource()->id());
}

void DirectRasterWorkerPool::ReleaseCanvasForRaster(RasterTask* task) {
  resource_provider_->UnmapDirectRasterBuffer(task->resource()->id());
}

void DirectRasterWorkerPool::OnRasterFinished() {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::OnRasterFinished");

  DCHECK(raster_tasks_pending_);
  raster_tasks_pending_ = false;
  client_->DidFinishRunningTasks();
}

void DirectRasterWorkerPool::OnRasterRequiredForActivationFinished() {
  TRACE_EVENT0("cc",
               "DirectRasterWorkerPool::OnRasterRequiredForActivationFinished");

  DCHECK(raster_tasks_required_for_activation_pending_);
  raster_tasks_required_for_activation_pending_ = false;
  client_->DidFinishRunningTasksRequiredForActivation();
}

void DirectRasterWorkerPool::ScheduleRunTasksOnOriginThread() {
  if (run_tasks_on_origin_thread_pending_)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DirectRasterWorkerPool::RunTasksOnOriginThread,
                 weak_ptr_factory_.GetWeakPtr()));
  run_tasks_on_origin_thread_pending_ = true;
}

void DirectRasterWorkerPool::RunTasksOnOriginThread() {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::RunTasksOnOriginThread");

  DCHECK(run_tasks_on_origin_thread_pending_);
  run_tasks_on_origin_thread_pending_ = false;

  if (context_provider_) {
    DCHECK(context_provider_->ContextGL());
    // TODO(alokp): Use a trace macro to push/pop markers.
    // Using push/pop functions directly incurs cost to evaluate function
    // arguments even when tracing is disabled.
    context_provider_->ContextGL()->PushGroupMarkerEXT(
        0, "DirectRasterWorkerPool::RunTasksOnOriginThread");

    GrContext* gr_context = context_provider_->GrContext();
    // TODO(alokp): Implement TestContextProvider::GrContext().
    if (gr_context)
      gr_context->resetContext();
  }

  task_graph_runner_->RunUntilIdle();

  if (context_provider_) {
    GrContext* gr_context = context_provider_->GrContext();
    // TODO(alokp): Implement TestContextProvider::GrContext().
    if (gr_context)
      gr_context->flush();

    context_provider_->ContextGL()->PopGroupMarkerEXT();
  }
}

}  // namespace cc
