// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_IMAGE_COPY_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_IMAGE_COPY_RASTER_WORKER_POOL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/rasterizer.h"

namespace cc {
class ResourcePool;
class ResourceProvider;
class ScopedResource;

class CC_EXPORT ImageCopyRasterWorkerPool : public RasterWorkerPool,
                                            public Rasterizer,
                                            public RasterizerTaskClient {
 public:
  virtual ~ImageCopyRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      TaskGraphRunner* task_graph_runner,
      ResourceProvider* resource_provider,
      ResourcePool* resource_pool);

  // Overridden from RasterWorkerPool:
  virtual Rasterizer* AsRasterizer() OVERRIDE;

  // Overridden from Rasterizer:
  virtual void SetClient(RasterizerClient* client) OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE;
  virtual void CheckForCompletedTasks() OVERRIDE;

  // Overridden from RasterizerTaskClient:
  virtual SkCanvas* AcquireCanvasForRaster(RasterTask* task) OVERRIDE;
  virtual void ReleaseCanvasForRaster(RasterTask* task) OVERRIDE;

 protected:
  ImageCopyRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                            TaskGraphRunner* task_graph_runner,
                            ResourceProvider* resource_provider,
                            ResourcePool* resource_pool);

 private:
  struct RasterTaskState {
    class TaskComparator {
     public:
      explicit TaskComparator(const RasterTask* task) : task_(task) {}

      bool operator()(const RasterTaskState& state) const {
        return state.task == task_;
      }

     private:
      const RasterTask* task_;
    };

    typedef std::vector<RasterTaskState> Vector;

    RasterTaskState(const RasterTask* task, ScopedResource* resource)
        : task(task), resource(resource) {}

    const RasterTask* task;
    ScopedResource* resource;
  };

  void OnRasterFinished();
  void OnRasterRequiredForActivationFinished();
  void FlushCopies();
  scoped_ptr<base::Value> StateAsValue() const;
  scoped_ptr<base::Value> StagingStateAsValue() const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
  RasterizerClient* client_;
  ResourceProvider* resource_provider_;
  ResourcePool* resource_pool_;

  RasterTaskState::Vector raster_task_states_;

  bool has_performed_copy_since_last_flush_;

  bool raster_tasks_pending_;
  bool raster_tasks_required_for_activation_pending_;

  base::WeakPtrFactory<ImageCopyRasterWorkerPool>
      raster_finished_weak_ptr_factory_;

  scoped_refptr<RasterizerTask> raster_finished_task_;
  scoped_refptr<RasterizerTask> raster_required_for_activation_finished_task_;

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  TaskGraph graph_;
  Task::Vector completed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(ImageCopyRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_IMAGE_COPY_RASTER_WORKER_POOL_H_
