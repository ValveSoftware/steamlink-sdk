// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "base/time/time.h"
#include "cc/debug/lap_timer.h"
#include "cc/output/context_provider.h"
#include "cc/resources/direct_raster_worker_pool.h"
#include "cc/resources/image_copy_raster_worker_pool.h"
#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_context_support.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {
namespace {

class PerfGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
  // Overridden from gpu::gles2::GLES2Interface:
  virtual GLuint CreateImageCHROMIUM(GLsizei width,
                                     GLsizei height,
                                     GLenum internalformat,
                                     GLenum usage) OVERRIDE {
    return 1u;
  }
  virtual void GenBuffers(GLsizei n, GLuint* buffers) OVERRIDE {
    for (GLsizei i = 0; i < n; ++i)
      buffers[i] = 1u;
  }
  virtual void GenTextures(GLsizei n, GLuint* textures) OVERRIDE {
    for (GLsizei i = 0; i < n; ++i)
      textures[i] = 1u;
  }
  virtual void GetIntegerv(GLenum pname, GLint* params) OVERRIDE {
    if (pname == GL_MAX_TEXTURE_SIZE)
      *params = INT_MAX;
  }
};

class PerfContextProvider : public ContextProvider {
 public:
  PerfContextProvider() : context_gl_(new PerfGLES2Interface) {}

  virtual bool BindToCurrentThread() OVERRIDE { return true; }
  virtual Capabilities ContextCapabilities() OVERRIDE { return Capabilities(); }
  virtual gpu::gles2::GLES2Interface* ContextGL() OVERRIDE {
    return context_gl_.get();
  }
  virtual gpu::ContextSupport* ContextSupport() OVERRIDE { return &support_; }
  virtual class GrContext* GrContext() OVERRIDE { return NULL; }
  virtual bool IsContextLost() OVERRIDE { return false; }
  virtual void VerifyContexts() OVERRIDE {}
  virtual void DeleteCachedResources() OVERRIDE {}
  virtual bool DestroyedOnMainThread() OVERRIDE { return false; }
  virtual void SetLostContextCallback(const LostContextCallback& cb) OVERRIDE {}
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& cb) OVERRIDE {}

 private:
  virtual ~PerfContextProvider() {}

  scoped_ptr<PerfGLES2Interface> context_gl_;
  TestContextSupport support_;
};

enum RasterWorkerPoolType {
  RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER,
  RASTER_WORKER_POOL_TYPE_IMAGE,
  RASTER_WORKER_POOL_TYPE_IMAGE_COPY,
  RASTER_WORKER_POOL_TYPE_DIRECT
};

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfImageDecodeTaskImpl : public ImageDecodeTask {
 public:
  PerfImageDecodeTaskImpl() {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {}

  // Overridden from RasterizerTask:
  virtual void ScheduleOnOriginThread(RasterizerTaskClient* client) OVERRIDE {}
  virtual void CompleteOnOriginThread(RasterizerTaskClient* client) OVERRIDE {}
  virtual void RunReplyOnOriginThread() OVERRIDE { Reset(); }

  void Reset() {
    did_run_ = false;
    did_complete_ = false;
  }

 protected:
  virtual ~PerfImageDecodeTaskImpl() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfImageDecodeTaskImpl);
};

class PerfRasterTaskImpl : public RasterTask {
 public:
  PerfRasterTaskImpl(scoped_ptr<ScopedResource> resource,
                     ImageDecodeTask::Vector* dependencies)
      : RasterTask(resource.get(), dependencies), resource_(resource.Pass()) {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {}

  // Overridden from RasterizerTask:
  virtual void ScheduleOnOriginThread(RasterizerTaskClient* client) OVERRIDE {
    client->AcquireCanvasForRaster(this);
  }
  virtual void CompleteOnOriginThread(RasterizerTaskClient* client) OVERRIDE {
    client->ReleaseCanvasForRaster(this);
  }
  virtual void RunReplyOnOriginThread() OVERRIDE { Reset(); }

  void Reset() {
    did_run_ = false;
    did_complete_ = false;
  }

 protected:
  virtual ~PerfRasterTaskImpl() {}

 private:
  scoped_ptr<ScopedResource> resource_;

  DISALLOW_COPY_AND_ASSIGN(PerfRasterTaskImpl);
};

class RasterWorkerPoolPerfTestBase {
 public:
  typedef std::vector<scoped_refptr<RasterTask> > RasterTaskVector;

  RasterWorkerPoolPerfTestBase()
      : context_provider_(make_scoped_refptr(new PerfContextProvider)),
        task_graph_runner_(new TaskGraphRunner),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ =
        ResourceProvider::Create(
            output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
            false).Pass();
    staging_resource_pool_ = ResourcePool::Create(
        resource_provider_.get(), GL_TEXTURE_2D, RGBA_8888);
  }

  void CreateImageDecodeTasks(unsigned num_image_decode_tasks,
                              ImageDecodeTask::Vector* image_decode_tasks) {
    for (unsigned i = 0; i < num_image_decode_tasks; ++i)
      image_decode_tasks->push_back(new PerfImageDecodeTaskImpl);
  }

  void CreateRasterTasks(unsigned num_raster_tasks,
                         const ImageDecodeTask::Vector& image_decode_tasks,
                         RasterTaskVector* raster_tasks) {
    const gfx::Size size(1, 1);

    for (unsigned i = 0; i < num_raster_tasks; ++i) {
      scoped_ptr<ScopedResource> resource(
          ScopedResource::Create(resource_provider_.get()));
      resource->Allocate(size, ResourceProvider::TextureUsageAny, RGBA_8888);

      ImageDecodeTask::Vector dependencies = image_decode_tasks;
      raster_tasks->push_back(
          new PerfRasterTaskImpl(resource.Pass(), &dependencies));
    }
  }

  void BuildRasterTaskQueue(RasterTaskQueue* queue,
                            const RasterTaskVector& raster_tasks) {
    for (size_t i = 0u; i < raster_tasks.size(); ++i) {
      bool required_for_activation = (i % 2) == 0;
      queue->items.push_back(RasterTaskQueue::Item(raster_tasks[i].get(),
                                                   required_for_activation));
      queue->required_for_activation_count += required_for_activation;
    }
  }

 protected:
  scoped_refptr<ContextProvider> context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourcePool> staging_resource_pool_;
  scoped_ptr<TaskGraphRunner> task_graph_runner_;
  LapTimer timer_;
};

class RasterWorkerPoolPerfTest
    : public RasterWorkerPoolPerfTestBase,
      public testing::TestWithParam<RasterWorkerPoolType>,
      public RasterizerClient {
 public:
  RasterWorkerPoolPerfTest() {
    switch (GetParam()) {
      case RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER:
        raster_worker_pool_ = PixelBufferRasterWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            task_graph_runner_.get(),
            resource_provider_.get(),
            std::numeric_limits<size_t>::max());
        break;
      case RASTER_WORKER_POOL_TYPE_IMAGE:
        raster_worker_pool_ = ImageRasterWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            task_graph_runner_.get(),
            resource_provider_.get());
        break;
      case RASTER_WORKER_POOL_TYPE_IMAGE_COPY:
        raster_worker_pool_ = ImageCopyRasterWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            task_graph_runner_.get(),
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

  // Overridden from testing::Test:
  virtual void TearDown() OVERRIDE {
    raster_worker_pool_->AsRasterizer()->Shutdown();
    raster_worker_pool_->AsRasterizer()->CheckForCompletedTasks();
  }

  // Overriden from RasterizerClient:
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
    task_graph_runner_->RunUntilIdle();
    base::MessageLoop::current()->Run();
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            unsigned num_raster_tasks,
                            unsigned num_image_decode_tasks) {
    ImageDecodeTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks);
      raster_worker_pool_->AsRasterizer()->ScheduleTasks(&queue);
      raster_worker_pool_->AsRasterizer()->CheckForCompletedTasks();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    RasterTaskQueue empty;
    raster_worker_pool_->AsRasterizer()->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_tasks",
                           TestModifierString(),
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunScheduleAlternateTasksTest(const std::string& test_name,
                                     unsigned num_raster_tasks,
                                     unsigned num_image_decode_tasks) {
    const size_t kNumVersions = 2;
    ImageDecodeTask::Vector image_decode_tasks[kNumVersions];
    RasterTaskVector raster_tasks[kNumVersions];
    for (size_t i = 0; i < kNumVersions; ++i) {
      CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks[i]);
      CreateRasterTasks(
          num_raster_tasks, image_decode_tasks[i], &raster_tasks[i]);
    }

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    size_t count = 0;
    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks[count % kNumVersions]);
      raster_worker_pool_->AsRasterizer()->ScheduleTasks(&queue);
      raster_worker_pool_->AsRasterizer()->CheckForCompletedTasks();
      ++count;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    RasterTaskQueue empty;
    raster_worker_pool_->AsRasterizer()->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_alternate_tasks",
                           TestModifierString(),
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunScheduleAndExecuteTasksTest(const std::string& test_name,
                                      unsigned num_raster_tasks,
                                      unsigned num_image_decode_tasks) {
    ImageDecodeTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks);
      raster_worker_pool_->AsRasterizer()->ScheduleTasks(&queue);
      RunMessageLoopUntilAllTasksHaveCompleted();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    RasterTaskQueue empty;
    raster_worker_pool_->AsRasterizer()->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_and_execute_tasks",
                           TestModifierString(),
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

 private:
  std::string TestModifierString() const {
    switch (GetParam()) {
      case RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER:
        return std::string("_pixel_raster_worker_pool");
      case RASTER_WORKER_POOL_TYPE_IMAGE:
        return std::string("_image_raster_worker_pool");
      case RASTER_WORKER_POOL_TYPE_IMAGE_COPY:
        return std::string("_image_copy_raster_worker_pool");
      case RASTER_WORKER_POOL_TYPE_DIRECT:
        return std::string("_direct_raster_worker_pool");
    }
    NOTREACHED();
    return std::string();
  }

  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
};

TEST_P(RasterWorkerPoolPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("1_0", 1, 0);
  RunScheduleTasksTest("32_0", 32, 0);
  RunScheduleTasksTest("1_1", 1, 1);
  RunScheduleTasksTest("32_1", 32, 1);
  RunScheduleTasksTest("1_4", 1, 4);
  RunScheduleTasksTest("32_4", 32, 4);
}

TEST_P(RasterWorkerPoolPerfTest, ScheduleAlternateTasks) {
  RunScheduleAlternateTasksTest("1_0", 1, 0);
  RunScheduleAlternateTasksTest("32_0", 32, 0);
  RunScheduleAlternateTasksTest("1_1", 1, 1);
  RunScheduleAlternateTasksTest("32_1", 32, 1);
  RunScheduleAlternateTasksTest("1_4", 1, 4);
  RunScheduleAlternateTasksTest("32_4", 32, 4);
}

TEST_P(RasterWorkerPoolPerfTest, ScheduleAndExecuteTasks) {
  RunScheduleAndExecuteTasksTest("1_0", 1, 0);
  RunScheduleAndExecuteTasksTest("32_0", 32, 0);
  RunScheduleAndExecuteTasksTest("1_1", 1, 1);
  RunScheduleAndExecuteTasksTest("32_1", 32, 1);
  RunScheduleAndExecuteTasksTest("1_4", 1, 4);
  RunScheduleAndExecuteTasksTest("32_4", 32, 4);
}

INSTANTIATE_TEST_CASE_P(RasterWorkerPoolPerfTests,
                        RasterWorkerPoolPerfTest,
                        ::testing::Values(RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER,
                                          RASTER_WORKER_POOL_TYPE_IMAGE,
                                          RASTER_WORKER_POOL_TYPE_IMAGE_COPY,
                                          RASTER_WORKER_POOL_TYPE_DIRECT));

class RasterWorkerPoolCommonPerfTest : public RasterWorkerPoolPerfTestBase,
                                       public testing::Test {
 public:
  void RunBuildRasterTaskQueueTest(const std::string& test_name,
                                   unsigned num_raster_tasks,
                                   unsigned num_image_decode_tasks) {
    ImageDecodeTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("build_raster_task_queue",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }
};

TEST_F(RasterWorkerPoolCommonPerfTest, BuildRasterTaskQueue) {
  RunBuildRasterTaskQueueTest("1_0", 1, 0);
  RunBuildRasterTaskQueueTest("32_0", 32, 0);
  RunBuildRasterTaskQueueTest("1_1", 1, 1);
  RunBuildRasterTaskQueueTest("32_1", 32, 1);
  RunBuildRasterTaskQueueTest("1_4", 1, 4);
  RunBuildRasterTaskQueueTest("32_4", 32, 4);
}

}  // namespace
}  // namespace cc
