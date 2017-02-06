// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_display_output_surface.h"

#include <memory>

#include "cc/output/renderer_settings.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class SurfaceDisplayOutputSurfaceTest : public testing::Test {
 public:
  SurfaceDisplayOutputSurfaceTest()
      : now_src_(new base::SimpleTestTickClock()),
        task_runner_(new OrderedSimpleTaskRunner(now_src_.get(), true)),
        allocator_(0),
        display_size_(1920, 1080),
        display_rect_(display_size_),
        context_provider_(TestContextProvider::Create()) {
    surface_manager_.RegisterSurfaceIdNamespace(allocator_.id_namespace());

    std::unique_ptr<FakeOutputSurface> display_output_surface =
        FakeOutputSurface::Create3d();
    display_output_surface_ = display_output_surface.get();

    std::unique_ptr<BeginFrameSource> begin_frame_source(
        new BackToBackBeginFrameSource(
            base::MakeUnique<DelayBasedTimeSource>(task_runner_.get())));

    int max_frames_pending = 2;
    std::unique_ptr<DisplayScheduler> scheduler(new DisplayScheduler(
        begin_frame_source.get(), task_runner_.get(), max_frames_pending));

    display_.reset(new Display(
        &surface_manager_, &bitmap_manager_, &gpu_memory_buffer_manager_,
        RendererSettings(), allocator_.id_namespace(),
        std::move(begin_frame_source), std::move(display_output_surface),
        std::move(scheduler),
        base::MakeUnique<TextureMailboxDeleter>(task_runner_.get())));
    delegated_output_surface_.reset(new SurfaceDisplayOutputSurface(
        &surface_manager_, &allocator_, display_.get(), context_provider_,
        nullptr));

    delegated_output_surface_->BindToClient(&delegated_output_surface_client_);
    display_->Resize(display_size_);

    EXPECT_FALSE(
        delegated_output_surface_client_.did_lose_output_surface_called());
  }

  ~SurfaceDisplayOutputSurfaceTest() override {}

  void SwapBuffersWithDamage(const gfx::Rect& damage_rect) {
    std::unique_ptr<RenderPass> render_pass(RenderPass::Create());
    render_pass->SetNew(RenderPassId(1, 1), display_rect_, damage_rect,
                        gfx::Transform());

    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    frame_data->render_pass_list.push_back(std::move(render_pass));

    CompositorFrame frame;
    frame.delegated_frame_data = std::move(frame_data);

    delegated_output_surface_->SwapBuffers(std::move(frame));
  }

  void SetUp() override {
    // Draw the first frame to start in an "unlocked" state.
    SwapBuffersWithDamage(display_rect_);

    EXPECT_EQ(0u, display_output_surface_->num_sent_frames());
    task_runner_->RunUntilIdle();
    EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<OrderedSimpleTaskRunner> task_runner_;
  SurfaceIdAllocator allocator_;

  const gfx::Size display_size_;
  const gfx::Rect display_rect_;
  SurfaceManager surface_manager_;
  TestSharedBitmapManager bitmap_manager_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;

  scoped_refptr<TestContextProvider> context_provider_;
  FakeOutputSurface* display_output_surface_ = nullptr;
  std::unique_ptr<Display> display_;
  FakeOutputSurfaceClient delegated_output_surface_client_;
  std::unique_ptr<SurfaceDisplayOutputSurface> delegated_output_surface_;
};

TEST_F(SurfaceDisplayOutputSurfaceTest, DamageTriggersSwapBuffers) {
  SwapBuffersWithDamage(display_rect_);
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, display_output_surface_->num_sent_frames());
}

TEST_F(SurfaceDisplayOutputSurfaceTest, NoDamageDoesNotTriggerSwapBuffers) {
  SwapBuffersWithDamage(gfx::Rect());
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
}

TEST_F(SurfaceDisplayOutputSurfaceTest, SuspendedDoesNotTriggerSwapBuffers) {
  SwapBuffersWithDamage(display_rect_);
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  display_output_surface_->set_suspended_for_recycle(true);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  SwapBuffersWithDamage(display_rect_);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  display_output_surface_->set_suspended_for_recycle(false);
  SwapBuffersWithDamage(display_rect_);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, display_output_surface_->num_sent_frames());
}

TEST_F(SurfaceDisplayOutputSurfaceTest,
       LockingResourcesDoesNotIndirectlyCauseDamage) {
  delegated_output_surface_->ForceReclaimResources();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunPendingTasks();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());

  SwapBuffersWithDamage(gfx::Rect());
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
}

}  // namespace
}  // namespace cc
