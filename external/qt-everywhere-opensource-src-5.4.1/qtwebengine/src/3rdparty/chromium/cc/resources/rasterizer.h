// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTERIZER_H_
#define CC_RESOURCES_RASTERIZER_H_

#include <vector>

#include "base/callback.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/task_graph_runner.h"

class SkCanvas;

namespace cc {
class ImageDecodeTask;
class RasterTask;
class Resource;

class CC_EXPORT RasterizerTaskClient {
 public:
  virtual SkCanvas* AcquireCanvasForRaster(RasterTask* task) = 0;
  virtual void ReleaseCanvasForRaster(RasterTask* task) = 0;

 protected:
  virtual ~RasterizerTaskClient() {}
};

class CC_EXPORT RasterizerTask : public Task {
 public:
  typedef std::vector<scoped_refptr<RasterizerTask> > Vector;

  virtual void ScheduleOnOriginThread(RasterizerTaskClient* client) = 0;
  virtual void CompleteOnOriginThread(RasterizerTaskClient* client) = 0;
  virtual void RunReplyOnOriginThread() = 0;

  // Type-checking downcast routines.
  virtual ImageDecodeTask* AsImageDecodeTask();
  virtual RasterTask* AsRasterTask();

  void WillSchedule();
  void DidSchedule();
  bool HasBeenScheduled() const;

  void WillComplete();
  void DidComplete();
  bool HasCompleted() const;

 protected:
  RasterizerTask();
  virtual ~RasterizerTask();

  bool did_schedule_;
  bool did_complete_;
};

class CC_EXPORT ImageDecodeTask : public RasterizerTask {
 public:
  typedef std::vector<scoped_refptr<ImageDecodeTask> > Vector;

  // Overridden from RasterizerTask:
  virtual ImageDecodeTask* AsImageDecodeTask() OVERRIDE;

 protected:
  ImageDecodeTask();
  virtual ~ImageDecodeTask();
};

class CC_EXPORT RasterTask : public RasterizerTask {
 public:
  typedef std::vector<scoped_refptr<RasterTask> > Vector;

  // Overridden from RasterizerTask:
  virtual RasterTask* AsRasterTask() OVERRIDE;

  const Resource* resource() const { return resource_; }
  const ImageDecodeTask::Vector& dependencies() const { return dependencies_; }

 protected:
  RasterTask(const Resource* resource, ImageDecodeTask::Vector* dependencies);
  virtual ~RasterTask();

 private:
  const Resource* resource_;
  ImageDecodeTask::Vector dependencies_;
};

class CC_EXPORT RasterizerClient {
 public:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const = 0;
  virtual void DidFinishRunningTasks() = 0;
  virtual void DidFinishRunningTasksRequiredForActivation() = 0;

 protected:
  virtual ~RasterizerClient() {}
};

struct CC_EXPORT RasterTaskQueue {
  struct CC_EXPORT Item {
    class TaskComparator {
     public:
      explicit TaskComparator(const RasterTask* task) : task_(task) {}

      bool operator()(const Item& item) const { return item.task == task_; }

     private:
      const RasterTask* task_;
    };

    typedef std::vector<Item> Vector;

    Item(RasterTask* task, bool required_for_activation);
    ~Item();

    static bool IsRequiredForActivation(const Item& item) {
      return item.required_for_activation;
    }

    RasterTask* task;
    bool required_for_activation;
  };

  RasterTaskQueue();
  ~RasterTaskQueue();

  void Swap(RasterTaskQueue* other);
  void Reset();

  Item::Vector items;
  size_t required_for_activation_count;
};

// This interface can be used to schedule and run raster tasks. The client will
// be notified asynchronously when the set of tasks marked as "required for
// activation" have finished running and when all scheduled tasks have finished
// running. The client can call CheckForCompletedTasks() at any time to dispatch
// pending completion callbacks for all tasks that have finished running.
class CC_EXPORT Rasterizer {
 public:
  // Set the client instance to be notified when finished running tasks.
  virtual void SetClient(RasterizerClient* client) = 0;

  // Tells the worker pool to shutdown after canceling all previously scheduled
  // tasks. Reply callbacks are still guaranteed to run when
  // CheckForCompletedTasks() is called.
  virtual void Shutdown() = 0;

  // Schedule running of raster tasks in |queue| and all dependencies.
  // Previously scheduled tasks that are not in |queue| will be canceled unless
  // already running. Once scheduled, reply callbacks are guaranteed to run for
  // all tasks even if they later get canceled by another call to
  // ScheduleTasks().
  virtual void ScheduleTasks(RasterTaskQueue* queue) = 0;

  // Check for completed tasks and dispatch reply callbacks.
  virtual void CheckForCompletedTasks() = 0;

 protected:
  virtual ~Rasterizer() {}
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTERIZER_H_
