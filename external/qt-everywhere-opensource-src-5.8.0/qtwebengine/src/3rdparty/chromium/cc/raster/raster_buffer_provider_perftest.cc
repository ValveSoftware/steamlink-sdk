// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "cc/debug/lap_timer.h"
#include "cc/output/context_provider.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_context_support.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/tiles/tile_task_manager.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace cc {
namespace {

class PerfGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
  // Overridden from gpu::gles2::GLES2Interface:
  GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                             GLsizei width,
                             GLsizei height,
                             GLenum internalformat) override {
    return 1u;
  }
  void GenBuffers(GLsizei n, GLuint* buffers) override {
    for (GLsizei i = 0; i < n; ++i)
      buffers[i] = 1u;
  }
  void GenTextures(GLsizei n, GLuint* textures) override {
    for (GLsizei i = 0; i < n; ++i)
      textures[i] = 1u;
  }
  void GetIntegerv(GLenum pname, GLint* params) override {
    if (pname == GL_MAX_TEXTURE_SIZE)
      *params = INT_MAX;
  }
  void GenQueriesEXT(GLsizei n, GLuint* queries) override {
    for (GLsizei i = 0; i < n; ++i)
      queries[i] = 1u;
  }
  void GetQueryObjectuivEXT(GLuint query,
                            GLenum pname,
                            GLuint* params) override {
    if (pname == GL_QUERY_RESULT_AVAILABLE_EXT)
      *params = 1;
  }
  void GenUnverifiedSyncTokenCHROMIUM(GLuint64 fence_sync,
                                      GLbyte* sync_token) override {
    // Copy the data over after setting the data to ensure alignment.
    gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO, 0,
                                   gpu::CommandBufferId(), fence_sync);
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }
};

class PerfContextProvider : public ContextProvider {
 public:
  PerfContextProvider() : context_gl_(new PerfGLES2Interface) {}

  bool BindToCurrentThread() override { return true; }
  gpu::Capabilities ContextCapabilities() override {
    gpu::Capabilities capabilities;
    capabilities.image = true;
    capabilities.sync_query = true;
    return capabilities;
  }
  gpu::gles2::GLES2Interface* ContextGL() override { return context_gl_.get(); }
  gpu::ContextSupport* ContextSupport() override { return &support_; }
  class GrContext* GrContext() override {
    if (gr_context_)
      return gr_context_.get();

    sk_sp<const GrGLInterface> null_interface(GrGLCreateNullInterface());
    gr_context_ = sk_sp<class GrContext>(GrContext::Create(
        kOpenGL_GrBackend,
        reinterpret_cast<GrBackendContext>(null_interface.get())));
    return gr_context_.get();
  }
  void InvalidateGrContext(uint32_t state) override {
    if (gr_context_)
      gr_context_.get()->resetContext(state);
  }
  base::Lock* GetLock() override { return &context_lock_; }
  void DeleteCachedResources() override {}
  void SetLostContextCallback(const LostContextCallback& cb) override {}

 private:
  ~PerfContextProvider() override {}

  std::unique_ptr<PerfGLES2Interface> context_gl_;
  sk_sp<class GrContext> gr_context_;
  TestContextSupport support_;
  base::Lock context_lock_;
};

enum RasterBufferProviderType {
  RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_GPU,
  RASTER_BUFFER_PROVIDER_TYPE_BITMAP
};

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfTileTask : public TileTask {
 public:
  PerfTileTask() : TileTask(true) {}
  explicit PerfTileTask(TileTask::Vector* dependencies)
      : TileTask(true, dependencies) {}

  void Reset() {
    did_complete_ = false;
    state().Reset();
  }

  void Cancel() {
    if (!state().IsCanceled())
      state().DidCancel();

    did_complete_ = true;
  }

 protected:
  ~PerfTileTask() override {}
};

class PerfImageDecodeTaskImpl : public PerfTileTask {
 public:
  PerfImageDecodeTaskImpl() {}

  // Overridden from Task:
  void RunOnWorkerThread() override {}

  // Overridden from TileTask:
  void OnTaskCompleted() override {}

 protected:
  ~PerfImageDecodeTaskImpl() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfImageDecodeTaskImpl);
};

class PerfRasterBufferProviderHelper {
 public:
  virtual std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) = 0;
  virtual void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) = 0;
};

class PerfRasterTaskImpl : public PerfTileTask {
 public:
  PerfRasterTaskImpl(PerfRasterBufferProviderHelper* helper,
                     std::unique_ptr<ScopedResource> resource,
                     std::unique_ptr<RasterBuffer> raster_buffer,
                     TileTask::Vector* dependencies)
      : PerfTileTask(dependencies),
        helper_(helper),
        resource_(std::move(resource)),
        raster_buffer_(std::move(raster_buffer)) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {}

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    if (helper_)
      helper_->ReleaseBufferForRaster(std::move(raster_buffer_));
  }

 protected:
  ~PerfRasterTaskImpl() override {}

 private:
  PerfRasterBufferProviderHelper* helper_;
  std::unique_ptr<ScopedResource> resource_;
  std::unique_ptr<RasterBuffer> raster_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PerfRasterTaskImpl);
};

class RasterBufferProviderPerfTestBase {
 public:
  typedef std::vector<scoped_refptr<TileTask>> RasterTaskVector;

  enum NamedTaskSet { REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW, ALL };

  RasterBufferProviderPerfTestBase()
      : compositor_context_provider_(
            make_scoped_refptr(new PerfContextProvider)),
        worker_context_provider_(make_scoped_refptr(new PerfContextProvider)),
        task_runner_(new base::TestSimpleTaskRunner),
        task_graph_runner_(new SynchronousTaskGraphRunner),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void CreateImageDecodeTasks(unsigned num_image_decode_tasks,
                              TileTask::Vector* image_decode_tasks) {
    for (unsigned i = 0; i < num_image_decode_tasks; ++i)
      image_decode_tasks->push_back(new PerfImageDecodeTaskImpl);
  }

  void CreateRasterTasks(PerfRasterBufferProviderHelper* helper,
                         unsigned num_raster_tasks,
                         const TileTask::Vector& image_decode_tasks,
                         RasterTaskVector* raster_tasks) {
    const gfx::Size size(1, 1);

    for (unsigned i = 0; i < num_raster_tasks; ++i) {
      std::unique_ptr<ScopedResource> resource(
          ScopedResource::Create(resource_provider_.get()));
      resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                         RGBA_8888);

      // No tile ids are given to support partial updates.
      std::unique_ptr<RasterBuffer> raster_buffer;
      if (helper)
        raster_buffer = helper->AcquireBufferForRaster(resource.get(), 0, 0);
      TileTask::Vector dependencies = image_decode_tasks;
      raster_tasks->push_back(
          new PerfRasterTaskImpl(helper, std::move(resource),
                                 std::move(raster_buffer), &dependencies));
    }
  }

  void ResetRasterTasks(const RasterTaskVector& raster_tasks) {
    for (auto& raster_task : raster_tasks) {
      for (auto& decode_task : raster_task->dependencies())
        static_cast<PerfTileTask*>(decode_task.get())->Reset();

      static_cast<PerfTileTask*>(raster_task.get())->Reset();
    }
  }

  void CancelRasterTasks(const RasterTaskVector& raster_tasks) {
    for (auto& raster_task : raster_tasks) {
      for (auto& decode_task : raster_task->dependencies())
        static_cast<PerfTileTask*>(decode_task.get())->Cancel();

      static_cast<PerfTileTask*>(raster_task.get())->Cancel();
    }
  }

  void BuildTileTaskGraph(TaskGraph* graph,
                          const RasterTaskVector& raster_tasks) {
    uint16_t priority = 0;

    for (auto& raster_task : raster_tasks) {
      priority++;

      for (auto& decode_task : raster_task->dependencies()) {
        // Add decode task if it doesn't already exist in graph.
        TaskGraph::Node::Vector::iterator decode_it =
            std::find_if(graph->nodes.begin(), graph->nodes.end(),
                         [decode_task](const TaskGraph::Node& node) {
                           return node.task == decode_task;
                         });

        if (decode_it == graph->nodes.end()) {
          graph->nodes.push_back(
              TaskGraph::Node(decode_task.get(), 0u /* group */, priority, 0u));
        }

        graph->edges.push_back(
            TaskGraph::Edge(decode_task.get(), raster_task.get()));
      }

      graph->nodes.push_back(TaskGraph::Node(
          raster_task.get(), 0u /* group */, priority,
          static_cast<uint32_t>(raster_task->dependencies().size())));
    }
  }

 protected:
  scoped_refptr<ContextProvider> compositor_context_provider_;
  scoped_refptr<ContextProvider> worker_context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<SynchronousTaskGraphRunner> task_graph_runner_;
  LapTimer timer_;
};

class RasterBufferProviderPerfTest
    : public RasterBufferProviderPerfTestBase,
      public PerfRasterBufferProviderHelper,
      public testing::TestWithParam<RasterBufferProviderType> {
 public:
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
            task_runner_.get(), compositor_context_provider_.get(),
            worker_context_provider_.get(), resource_provider_.get(),
            std::numeric_limits<int>::max(), false,
            std::numeric_limits<int>::max(), PlatformColor::BestTextureFormat(),
            false);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        Create3dOutputSurfaceAndResourceProvider();
        raster_buffer_provider_ = base::MakeUnique<GpuRasterBufferProvider>(
            compositor_context_provider_.get(), worker_context_provider_.get(),
            resource_provider_.get(), false, 0, false);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
        CreateSoftwareOutputSurfaceAndResourceProvider();
        raster_buffer_provider_ =
            BitmapRasterBufferProvider::Create(resource_provider_.get());
        break;
    }

    DCHECK(raster_buffer_provider_);

    tile_task_manager_ = TileTaskManagerImpl::Create(task_graph_runner_.get());
  }
  void TearDown() override {
    tile_task_manager_->Shutdown();
    tile_task_manager_->CheckForCompletedTasks();

    raster_buffer_provider_->Shutdown();
  }

  // Overridden from PerfRasterBufferProviderHelper:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override {
    return raster_buffer_provider_->AcquireBufferForRaster(
        resource, resource_content_id, previous_content_id);
  }
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override {
    raster_buffer_provider_->ReleaseBufferForRaster(std::move(buffer));
  }

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    task_graph_runner_->RunUntilIdle();
    task_runner_->RunUntilIdle();
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            unsigned num_raster_tasks,
                            unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(this, num_raster_tasks, image_decode_tasks,
                      &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      ResetRasterTasks(raster_tasks);
      BuildTileTaskGraph(&graph, raster_tasks);
      raster_buffer_provider_->OrderingBarrier();
      tile_task_manager_->ScheduleTasks(&graph);
      tile_task_manager_->CheckForCompletedTasks();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    raster_buffer_provider_->OrderingBarrier();
    tile_task_manager_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();
    tile_task_manager_->CheckForCompletedTasks();

    perf_test::PrintResult("schedule_tasks", TestModifierString(), test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunScheduleAlternateTasksTest(const std::string& test_name,
                                     unsigned num_raster_tasks,
                                     unsigned num_image_decode_tasks) {
    const size_t kNumVersions = 2;
    TileTask::Vector image_decode_tasks[kNumVersions];
    RasterTaskVector raster_tasks[kNumVersions];
    for (size_t i = 0; i < kNumVersions; ++i) {
      CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks[i]);
      CreateRasterTasks(this, num_raster_tasks, image_decode_tasks[i],
                        &raster_tasks[i]);
    }

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    size_t count = 0;
    timer_.Reset();
    do {
      graph.Reset();
      // Reset the tasks as for scheduling new state tasks are needed.
      ResetRasterTasks(raster_tasks[count % kNumVersions]);
      BuildTileTaskGraph(&graph, raster_tasks[count % kNumVersions]);
      raster_buffer_provider_->OrderingBarrier();
      tile_task_manager_->ScheduleTasks(&graph);
      tile_task_manager_->CheckForCompletedTasks();
      ++count;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    raster_buffer_provider_->OrderingBarrier();
    tile_task_manager_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();
    tile_task_manager_->CheckForCompletedTasks();

    perf_test::PrintResult("schedule_alternate_tasks", TestModifierString(),
                           test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunScheduleAndExecuteTasksTest(const std::string& test_name,
                                      unsigned num_raster_tasks,
                                      unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(this, num_raster_tasks, image_decode_tasks,
                      &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks);
      raster_buffer_provider_->OrderingBarrier();
      tile_task_manager_->ScheduleTasks(&graph);
      RunMessageLoopUntilAllTasksHaveCompleted();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    raster_buffer_provider_->OrderingBarrier();
    tile_task_manager_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_and_execute_tasks", TestModifierString(),
                           test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

 private:
  void Create3dOutputSurfaceAndResourceProvider() {
    output_surface_ = FakeOutputSurface::Create3d(compositor_context_provider_,
                                                  worker_context_provider_);
    CHECK(output_surface_->BindToClient(&output_surface_client_));
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

  std::string TestModifierString() const {
    switch (GetParam()) {
      case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
        return std::string("_zero_copy_raster_buffer_provider");
      case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
        return std::string("_one_copy_raster_buffer_provider");
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        return std::string("_gpu_raster_buffer_provider");
      case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
        return std::string("_bitmap_raster_buffer_provider");
    }
    NOTREACHED();
    return std::string();
  }

  std::unique_ptr<TileTaskManager> tile_task_manager_;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestSharedBitmapManager shared_bitmap_manager_;
};

TEST_P(RasterBufferProviderPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("1_0", 1, 0);
  RunScheduleTasksTest("32_0", 32, 0);
  RunScheduleTasksTest("1_1", 1, 1);
  RunScheduleTasksTest("32_1", 32, 1);
  RunScheduleTasksTest("1_4", 1, 4);
  RunScheduleTasksTest("32_4", 32, 4);
}

TEST_P(RasterBufferProviderPerfTest, ScheduleAlternateTasks) {
  RunScheduleAlternateTasksTest("1_0", 1, 0);
  RunScheduleAlternateTasksTest("32_0", 32, 0);
  RunScheduleAlternateTasksTest("1_1", 1, 1);
  RunScheduleAlternateTasksTest("32_1", 32, 1);
  RunScheduleAlternateTasksTest("1_4", 1, 4);
  RunScheduleAlternateTasksTest("32_4", 32, 4);
}

TEST_P(RasterBufferProviderPerfTest, ScheduleAndExecuteTasks) {
  RunScheduleAndExecuteTasksTest("1_0", 1, 0);
  RunScheduleAndExecuteTasksTest("32_0", 32, 0);
  RunScheduleAndExecuteTasksTest("1_1", 1, 1);
  RunScheduleAndExecuteTasksTest("32_1", 32, 1);
  RunScheduleAndExecuteTasksTest("1_4", 1, 4);
  RunScheduleAndExecuteTasksTest("32_4", 32, 4);
}

INSTANTIATE_TEST_CASE_P(RasterBufferProviderPerfTests,
                        RasterBufferProviderPerfTest,
                        ::testing::Values(RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
                                          RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
                                          RASTER_BUFFER_PROVIDER_TYPE_GPU,
                                          RASTER_BUFFER_PROVIDER_TYPE_BITMAP));

class RasterBufferProviderCommonPerfTest
    : public RasterBufferProviderPerfTestBase,
      public testing::Test {
 public:
  // Overridden from testing::Test:
  void SetUp() override {
    output_surface_ = FakeOutputSurface::Create3d(compositor_context_provider_,
                                                  worker_context_provider_);
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ =
        FakeResourceProvider::Create(output_surface_.get(), nullptr);
  }

  void RunBuildTileTaskGraphTest(const std::string& test_name,
                                 unsigned num_raster_tasks,
                                 unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(nullptr, num_raster_tasks, image_decode_tasks,
                      &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    CancelRasterTasks(raster_tasks);

    perf_test::PrintResult("build_raster_task_graph", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }
};

TEST_F(RasterBufferProviderCommonPerfTest, BuildTileTaskGraph) {
  RunBuildTileTaskGraphTest("1_0", 1, 0);
  RunBuildTileTaskGraphTest("32_0", 32, 0);
  RunBuildTileTaskGraphTest("1_1", 1, 1);
  RunBuildTileTaskGraphTest("32_1", 32, 1);
  RunBuildTileTaskGraphTest("1_4", 1, 4);
  RunBuildTileTaskGraphTest("32_4", 32, 4);
}

}  // namespace
}  // namespace cc
