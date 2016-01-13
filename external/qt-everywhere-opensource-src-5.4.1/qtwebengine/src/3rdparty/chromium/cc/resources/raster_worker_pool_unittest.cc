// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include <limits>
#include <vector>

#include "base/cancelable_callback.h"
#include "cc/resources/direct_raster_worker_pool.h"
#include "cc/resources/image_copy_raster_worker_pool.h"
#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/picture_pile.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

enum RasterWorkerPoolType {
  RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER,
  RASTER_WORKER_POOL_TYPE_IMAGE,
  RASTER_WORKER_POOL_TYPE_IMAGE_COPY,
  RASTER_WORKER_POOL_TYPE_DIRECT
};

class TestRasterTaskImpl : public RasterTask {
 public:
  typedef base::Callback<
      void(const PicturePileImpl::Analysis& analysis, bool was_canceled)> Reply;

  TestRasterTaskImpl(const Resource* resource,
                     const Reply& reply,
                     ImageDecodeTask::Vector* dependencies)
      : RasterTask(resource, dependencies), reply_(reply) {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {}

  // Overridden from RasterizerTask:
  virtual void ScheduleOnOriginThread(RasterizerTaskClient* client) OVERRIDE {
    client->AcquireCanvasForRaster(this);
  }
  virtual void CompleteOnOriginThread(RasterizerTaskClient* client) OVERRIDE {
    client->ReleaseCanvasForRaster(this);
  }
  virtual void RunReplyOnOriginThread() OVERRIDE {
    reply_.Run(PicturePileImpl::Analysis(), !HasFinishedRunning());
  }

 protected:
  virtual ~TestRasterTaskImpl() {}

 private:
  const Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(TestRasterTaskImpl);
};

class BlockingTestRasterTaskImpl : public TestRasterTaskImpl {
 public:
  BlockingTestRasterTaskImpl(const Resource* resource,
                             const Reply& reply,
                             base::Lock* lock,
                             ImageDecodeTask::Vector* dependencies)
      : TestRasterTaskImpl(resource, reply, dependencies), lock_(lock) {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {
    base::AutoLock lock(*lock_);
    TestRasterTaskImpl::RunOnWorkerThread();
  }

  // Overridden from RasterizerTask:
  virtual void RunReplyOnOriginThread() OVERRIDE {}

 protected:
  virtual ~BlockingTestRasterTaskImpl() {}

 private:
  base::Lock* lock_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTestRasterTaskImpl);
};

class RasterWorkerPoolTest
    : public testing::TestWithParam<RasterWorkerPoolType>,
      public RasterizerClient {
 public:
  struct RasterTaskResult {
    unsigned id;
    bool canceled;
  };

  typedef std::vector<scoped_refptr<RasterTask> > RasterTaskVector;

  RasterWorkerPoolTest()
      : context_provider_(TestContextProvider::Create()),
        timeout_seconds_(5),
        timed_out_(false) {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ =
        ResourceProvider::Create(
            output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
            false).Pass();
    staging_resource_pool_ = ResourcePool::Create(
        resource_provider_.get(), GL_TEXTURE_2D, RGBA_8888);

    switch (GetParam()) {
      case RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER:
        raster_worker_pool_ = PixelBufferRasterWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            RasterWorkerPool::GetTaskGraphRunner(),
            resource_provider_.get(),
            std::numeric_limits<size_t>::max());
        break;
      case RASTER_WORKER_POOL_TYPE_IMAGE:
        raster_worker_pool_ = ImageRasterWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            RasterWorkerPool::GetTaskGraphRunner(),
            resource_provider_.get());
        break;
      case RASTER_WORKER_POOL_TYPE_IMAGE_COPY:
        raster_worker_pool_ = ImageCopyRasterWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            RasterWorkerPool::GetTaskGraphRunner(),
            resource_provider_.get(),
            staging_resource_pool_.get());
        break;
      case RASTER_WORKER_POOL_TYPE_DIRECT:
        raster_worker_pool_ = DirectRasterWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            resource_provider_.get(),
            context_provider_.get());
        break;
    }

    DCHECK(raster_worker_pool_);
    raster_worker_pool_->AsRasterizer()->SetClient(this);
  }
  virtual ~RasterWorkerPoolTest() {
    staging_resource_pool_.reset();
    resource_provider_.reset();
  }

  // Overridden from testing::Test:
  virtual void TearDown() OVERRIDE {
    raster_worker_pool_->AsRasterizer()->Shutdown();
    raster_worker_pool_->AsRasterizer()->CheckForCompletedTasks();
  }

  // Overriden from RasterWorkerPoolClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const
      OVERRIDE {
    return false;
  }
  virtual void DidFinishRunningTasks() OVERRIDE {
    raster_worker_pool_->AsRasterizer()->CheckForCompletedTasks();
    base::MessageLoop::current()->Quit();
  }
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE {}

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    if (timeout_seconds_) {
      timeout_.Reset(
          base::Bind(&RasterWorkerPoolTest::OnTimeout, base::Unretained(this)));
      base::MessageLoopProxy::current()->PostDelayedTask(
          FROM_HERE,
          timeout_.callback(),
          base::TimeDelta::FromSeconds(timeout_seconds_));
    }

    base::MessageLoop::current()->Run();

    timeout_.Cancel();

    ASSERT_FALSE(timed_out_) << "Test timed out";
  }

  void ScheduleTasks() {
    RasterTaskQueue queue;

    for (RasterTaskVector::const_iterator it = tasks_.begin();
         it != tasks_.end();
         ++it)
      queue.items.push_back(RasterTaskQueue::Item(*it, false));

    raster_worker_pool_->AsRasterizer()->ScheduleTasks(&queue);
  }

  void AppendTask(unsigned id) {
    const gfx::Size size(1, 1);

    scoped_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TextureUsageAny, RGBA_8888);
    const Resource* const_resource = resource.get();

    ImageDecodeTask::Vector empty;
    tasks_.push_back(new TestRasterTaskImpl(
        const_resource,
        base::Bind(&RasterWorkerPoolTest::OnTaskCompleted,
                   base::Unretained(this),
                   base::Passed(&resource),
                   id),
        &empty));
  }

  void AppendBlockingTask(unsigned id, base::Lock* lock) {
    const gfx::Size size(1, 1);

    scoped_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TextureUsageAny, RGBA_8888);
    const Resource* const_resource = resource.get();

    ImageDecodeTask::Vector empty;
    tasks_.push_back(new BlockingTestRasterTaskImpl(
        const_resource,
        base::Bind(&RasterWorkerPoolTest::OnTaskCompleted,
                   base::Unretained(this),
                   base::Passed(&resource),
                   id),
        lock,
        &empty));
  }

  const std::vector<RasterTaskResult>& completed_tasks() const {
    return completed_tasks_;
  }

 private:
  void OnTaskCompleted(scoped_ptr<ScopedResource> resource,
                       unsigned id,
                       const PicturePileImpl::Analysis& analysis,
                       bool was_canceled) {
    RasterTaskResult result;
    result.id = id;
    result.canceled = was_canceled;
    completed_tasks_.push_back(result);
  }

  void OnTimeout() {
    timed_out_ = true;
    base::MessageLoop::current()->Quit();
  }

 protected:
  scoped_refptr<TestContextProvider> context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourcePool> staging_resource_pool_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  base::CancelableClosure timeout_;
  int timeout_seconds_;
  bool timed_out_;
  RasterTaskVector tasks_;
  std::vector<RasterTaskResult> completed_tasks_;
};

TEST_P(RasterWorkerPoolTest, Basic) {
  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(RasterWorkerPoolTest, FailedMapResource) {
  TestWebGraphicsContext3D* context3d = context_provider_->TestContext3d();
  context3d->set_times_map_image_chromium_succeeds(0);
  context3d->set_times_map_buffer_chromium_succeeds(0);
  AppendTask(0u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(1u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
}

// This test checks that replacing a pending raster task with another does
// not prevent the DidFinishRunningTasks notification from being sent.
TEST_P(RasterWorkerPoolTest, FalseThrottling) {
  base::Lock lock;

  // Schedule a task that is prevented from completing with a lock.
  lock.Acquire();
  AppendBlockingTask(0u, &lock);
  ScheduleTasks();

  // Schedule another task to replace the still-pending task. Because the old
  // task is not a throttled task in the new task set, it should not prevent
  // DidFinishRunningTasks from getting signaled.
  RasterTaskVector tasks;
  tasks.swap(tasks_);
  AppendTask(1u);
  ScheduleTasks();

  // Unblock the first task to allow the second task to complete.
  lock.Release();

  RunMessageLoopUntilAllTasksHaveCompleted();
}

INSTANTIATE_TEST_CASE_P(RasterWorkerPoolTests,
                        RasterWorkerPoolTest,
                        ::testing::Values(RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER,
                                          RASTER_WORKER_POOL_TYPE_IMAGE,
                                          RASTER_WORKER_POOL_TYPE_IMAGE_COPY,
                                          RASTER_WORKER_POOL_TYPE_DIRECT));

}  // namespace
}  // namespace cc
