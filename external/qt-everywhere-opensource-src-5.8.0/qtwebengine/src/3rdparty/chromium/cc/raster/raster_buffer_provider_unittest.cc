// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/unique_notifier.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/tiles/tile_task_manager.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const size_t kMaxBytesPerCopyOperation = 1000U;
const size_t kMaxStagingBuffers = 32U;

enum RasterBufferProviderType {
  RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_GPU,
  RASTER_BUFFER_PROVIDER_TYPE_BITMAP
};

class TestRasterTaskCompletionHandler {
 public:
  virtual void OnRasterTaskCompleted(
      std::unique_ptr<RasterBuffer> raster_buffer,
      unsigned id,
      bool was_canceled) = 0;
};

class TestRasterTaskImpl : public TileTask {
 public:
  TestRasterTaskImpl(TestRasterTaskCompletionHandler* completion_handler,
                     std::unique_ptr<ScopedResource> resource,
                     unsigned id,
                     std::unique_ptr<RasterBuffer> raster_buffer,
                     TileTask::Vector* dependencies)
      : TileTask(true, dependencies),
        completion_handler_(completion_handler),
        resource_(std::move(resource)),
        id_(id),
        raster_buffer_(std::move(raster_buffer)),
        raster_source_(FakeRasterSource::CreateFilled(gfx::Size(1, 1))) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    // Don't use the image hijack canvas for these tests, as they have no image
    // decode controller.
    RasterSource::PlaybackSettings settings;
    settings.use_image_hijack_canvas = false;

    uint64_t new_content_id = 0;
    raster_buffer_->Playback(raster_source_.get(), gfx::Rect(1, 1),
                             gfx::Rect(1, 1), new_content_id, 1.f, settings);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    completion_handler_->OnRasterTaskCompleted(std::move(raster_buffer_), id_,
                                               state().IsCanceled());
  }

 protected:
  ~TestRasterTaskImpl() override {}

 private:
  TestRasterTaskCompletionHandler* completion_handler_;
  std::unique_ptr<ScopedResource> resource_;
  unsigned id_;
  std::unique_ptr<RasterBuffer> raster_buffer_;
  scoped_refptr<RasterSource> raster_source_;

  DISALLOW_COPY_AND_ASSIGN(TestRasterTaskImpl);
};

class BlockingTestRasterTaskImpl : public TestRasterTaskImpl {
 public:
  BlockingTestRasterTaskImpl(
      TestRasterTaskCompletionHandler* completion_handler,
      std::unique_ptr<ScopedResource> resource,
      unsigned id,
      std::unique_ptr<RasterBuffer> raster_buffer,
      base::Lock* lock,
      TileTask::Vector* dependencies)
      : TestRasterTaskImpl(completion_handler,
                           std::move(resource),
                           id,
                           std::move(raster_buffer),
                           dependencies),
        lock_(lock) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    base::AutoLock lock(*lock_);
    TestRasterTaskImpl::RunOnWorkerThread();
  }

 protected:
  ~BlockingTestRasterTaskImpl() override {}

 private:
  base::Lock* lock_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTestRasterTaskImpl);
};

class RasterBufferProviderTest
    : public TestRasterTaskCompletionHandler,
      public testing::TestWithParam<RasterBufferProviderType> {
 public:
  struct RasterTaskResult {
    unsigned id;
    bool canceled;
  };

  typedef std::vector<scoped_refptr<TileTask>> RasterTaskVector;

  enum NamedTaskSet { REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW, ALL };

  RasterBufferProviderTest()
      : context_provider_(TestContextProvider::Create()),
        worker_context_provider_(TestContextProvider::CreateWorker()),
        all_tile_tasks_finished_(
            base::ThreadTaskRunnerHandle::Get().get(),
            base::Bind(&RasterBufferProviderTest::AllTileTasksFinished,
                       base::Unretained(this))),
        timeout_seconds_(5),
        timed_out_(false) {}

  // Overridden from testing::Test:
  void SetUp() override {
    switch (GetParam()) {
      case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        raster_buffer_provider_ = ZeroCopyRasterBufferProvider::Create(
            resource_provider_.get(), PlatformColor::BestTextureFormat());
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        raster_buffer_provider_ = base::MakeUnique<OneCopyRasterBufferProvider>(
            base::ThreadTaskRunnerHandle::Get().get(), context_provider_.get(),
            worker_context_provider_.get(), resource_provider_.get(),
            kMaxBytesPerCopyOperation, false, kMaxStagingBuffers,
            PlatformColor::BestTextureFormat(), false);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        Create3dOutputSurfaceAndResourceProvider();
        raster_buffer_provider_ = base::MakeUnique<GpuRasterBufferProvider>(
            context_provider_.get(), worker_context_provider_.get(),
            resource_provider_.get(), false, 0, false);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
        CreateSoftwareOutputSurfaceAndResourceProvider();
        raster_buffer_provider_ =
            BitmapRasterBufferProvider::Create(resource_provider_.get());
        break;
    }

    DCHECK(raster_buffer_provider_);

    tile_task_manager_ = TileTaskManagerImpl::Create(&task_graph_runner_);
  }

  void TearDown() override {
    tile_task_manager_->Shutdown();
    tile_task_manager_->CheckForCompletedTasks();

    raster_buffer_provider_->Shutdown();
  }

  void AllTileTasksFinished() {
    tile_task_manager_->CheckForCompletedTasks();
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    task_graph_runner_.RunUntilIdle();
    tile_task_manager_->CheckForCompletedTasks();
  }

  void ScheduleTasks() {
    graph_.Reset();

    size_t priority = 0;

    for (RasterTaskVector::const_iterator it = tasks_.begin();
         it != tasks_.end(); ++it) {
      graph_.nodes.emplace_back(it->get(), 0 /* group */, priority++,
                                0 /* dependencies */);
    }

    raster_buffer_provider_->OrderingBarrier();
    tile_task_manager_->ScheduleTasks(&graph_);
  }

  void AppendTask(unsigned id, const gfx::Size& size) {
    std::unique_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                       RGBA_8888);

    // The raster buffer has no tile ids associated with it for partial update,
    // so doesn't need to provide a valid dirty rect.
    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(resource.get(), 0, 0);
    TileTask::Vector empty;
    tasks_.push_back(new TestRasterTaskImpl(this, std::move(resource), id,
                                            std::move(raster_buffer), &empty));
  }

  void AppendTask(unsigned id) { AppendTask(id, gfx::Size(1, 1)); }

  void AppendBlockingTask(unsigned id, base::Lock* lock) {
    const gfx::Size size(1, 1);

    std::unique_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                       RGBA_8888);

    std::unique_ptr<RasterBuffer> raster_buffer =
        raster_buffer_provider_->AcquireBufferForRaster(resource.get(), 0, 0);
    TileTask::Vector empty;
    tasks_.push_back(new BlockingTestRasterTaskImpl(
        this, std::move(resource), id, std::move(raster_buffer), lock, &empty));
  }

  const std::vector<RasterTaskResult>& completed_tasks() const {
    return completed_tasks_;
  }

  void LoseContext(ContextProvider* context_provider) {
    if (!context_provider)
      return;
    context_provider->ContextGL()->LoseContextCHROMIUM(
        GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    context_provider->ContextGL()->Flush();
  }

  void OnRasterTaskCompleted(std::unique_ptr<RasterBuffer> raster_buffer,
                             unsigned id,
                             bool was_canceled) override {
    raster_buffer_provider_->ReleaseBufferForRaster(std::move(raster_buffer));
    RasterTaskResult result;
    result.id = id;
    result.canceled = was_canceled;
    completed_tasks_.push_back(result);
  }

 private:
  void Create3dOutputSurfaceAndResourceProvider() {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_,
                                                  worker_context_provider_);
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    TestWebGraphicsContext3D* context3d = context_provider_->TestContext3d();
    context3d->set_support_sync_query(true);
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), nullptr, &gpu_memory_buffer_manager_);
  }

  void CreateSoftwareOutputSurfaceAndResourceProvider() {
    output_surface_ = FakeOutputSurface::CreateSoftware(
        base::WrapUnique(new SoftwareOutputDevice));
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), &shared_bitmap_manager_, nullptr);
  }

  void OnTimeout() {
    timed_out_ = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }

 protected:
  scoped_refptr<TestContextProvider> context_provider_;
  scoped_refptr<TestContextProvider> worker_context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<TileTaskManager> tile_task_manager_;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestSharedBitmapManager shared_bitmap_manager_;
  SynchronousTaskGraphRunner task_graph_runner_;
  base::CancelableClosure timeout_;
  UniqueNotifier all_tile_tasks_finished_;
  int timeout_seconds_;
  bool timed_out_;
  RasterTaskVector tasks_;
  std::vector<RasterTaskResult> completed_tasks_;
  TaskGraph graph_;
};

TEST_P(RasterBufferProviderTest, Basic) {
  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(RasterBufferProviderTest, FailedMapResource) {
  if (GetParam() == RASTER_BUFFER_PROVIDER_TYPE_BITMAP)
    return;

  TestWebGraphicsContext3D* context3d = context_provider_->TestContext3d();
  context3d->set_times_map_buffer_chromium_succeeds(0);
  AppendTask(0u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(1u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
}

// This test checks that replacing a pending raster task with another does
// not prevent the DidFinishRunningTileTasks notification from being sent.
TEST_P(RasterBufferProviderTest, FalseThrottling) {
  base::Lock lock;

  // Schedule a task that is prevented from completing with a lock.
  lock.Acquire();
  AppendBlockingTask(0u, &lock);
  ScheduleTasks();

  // Schedule another task to replace the still-pending task. Because the old
  // task is not a throttled task in the new task set, it should not prevent
  // DidFinishRunningTileTasks from getting signaled.
  RasterTaskVector tasks;
  tasks.swap(tasks_);
  AppendTask(1u);
  ScheduleTasks();

  // Unblock the first task to allow the second task to complete.
  lock.Release();

  RunMessageLoopUntilAllTasksHaveCompleted();
}

TEST_P(RasterBufferProviderTest, LostContext) {
  LoseContext(output_surface_->context_provider());
  LoseContext(output_surface_->worker_context_provider());

  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

INSTANTIATE_TEST_CASE_P(RasterBufferProviderTests,
                        RasterBufferProviderTest,
                        ::testing::Values(RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
                                          RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
                                          RASTER_BUFFER_PROVIDER_TYPE_GPU,
                                          RASTER_BUFFER_PROVIDER_TYPE_BITMAP));

}  // namespace
}  // namespace cc
