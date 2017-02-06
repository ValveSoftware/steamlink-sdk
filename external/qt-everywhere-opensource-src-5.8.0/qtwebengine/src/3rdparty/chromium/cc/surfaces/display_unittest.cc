// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display.h"

#include <utility>

#include "base/test/null_task_runner.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;

namespace cc {
namespace {

class FakeSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  FakeSurfaceFactoryClient() : begin_frame_source_(nullptr) {}

  void ReturnResources(const ReturnedResourceArray& resources) override {}

  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {
    begin_frame_source_ = begin_frame_source;
  }

  BeginFrameSource* begin_frame_source() { return begin_frame_source_; }

 private:
  BeginFrameSource* begin_frame_source_;
};

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  TestSoftwareOutputDevice() {}

  gfx::Rect damage_rect() const { return damage_rect_; }
  gfx::Size viewport_pixel_size() const { return viewport_pixel_size_; }
};

class TestDisplayScheduler : public DisplayScheduler {
 public:
  TestDisplayScheduler(BeginFrameSource* begin_frame_source,
                       base::SingleThreadTaskRunner* task_runner)
      : DisplayScheduler(begin_frame_source, task_runner, 1),
        damaged(false),
        display_resized_(false),
        has_new_root_surface(false),
        swapped(false) {}

  ~TestDisplayScheduler() override {}

  void DisplayResized() override { display_resized_ = true; }

  void SetNewRootSurface(SurfaceId root_surface_id) override {
    has_new_root_surface = true;
  }

  void SurfaceDamaged(SurfaceId surface_id) override {
    damaged = true;
    needs_draw_ = true;
  }

  void DidSwapBuffers() override { swapped = true; }

  void ResetDamageForTest() {
    damaged = false;
    display_resized_ = false;
    has_new_root_surface = false;
  }

  bool damaged;
  bool display_resized_;
  bool has_new_root_surface;
  bool swapped;
};

class DisplayTest : public testing::Test {
 public:
  DisplayTest()
      : factory_(&manager_, &surface_factory_client_),
        id_allocator_(kArbitrarySurfaceNamespace),
        task_runner_(new base::NullTaskRunner) {
    id_allocator_.RegisterSurfaceIdNamespace(&manager_);
  }

  void SetUpDisplay(const RendererSettings& settings,
                    std::unique_ptr<TestWebGraphicsContext3D> context) {
    std::unique_ptr<BeginFrameSource> begin_frame_source(
        new StubBeginFrameSource);

    std::unique_ptr<FakeOutputSurface> output_surface;
    if (context) {
      output_surface = FakeOutputSurface::Create3d(std::move(context));
    } else {
      std::unique_ptr<TestSoftwareOutputDevice> device(
          new TestSoftwareOutputDevice);
      software_output_device_ = device.get();
      output_surface = FakeOutputSurface::CreateSoftware(std::move(device));
    }
    output_surface_ = output_surface.get();

    std::unique_ptr<TestDisplayScheduler> scheduler(
        new TestDisplayScheduler(begin_frame_source.get(), task_runner_.get()));
    scheduler_ = scheduler.get();

    display_ = base::MakeUnique<Display>(
        &manager_, &shared_bitmap_manager_,
        nullptr /* gpu_memory_buffer_manager */, settings,
        id_allocator_.id_namespace(), std::move(begin_frame_source),
        std::move(output_surface), std::move(scheduler),
        base::MakeUnique<TextureMailboxDeleter>(task_runner_.get()));
  }

 protected:
  void SubmitCompositorFrame(RenderPassList* pass_list, SurfaceId surface_id) {
    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    pass_list->swap(frame_data->render_pass_list);

    CompositorFrame frame;
    frame.delegated_frame_data = std::move(frame_data);

    factory_.SubmitCompositorFrame(surface_id, std::move(frame),
                                   SurfaceFactory::DrawCallback());
  }

  static constexpr int kArbitrarySurfaceNamespace = 3;

  SurfaceManager manager_;
  FakeSurfaceFactoryClient surface_factory_client_;
  SurfaceFactory factory_;
  SurfaceIdAllocator id_allocator_;
  scoped_refptr<base::NullTaskRunner> task_runner_;
  TestSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<Display> display_;
  TestSoftwareOutputDevice* software_output_device_ = nullptr;
  FakeOutputSurface* output_surface_ = nullptr;
  TestDisplayScheduler* scheduler_ = nullptr;
};

class StubDisplayClient : public DisplayClient {
 public:
  void DisplayOutputSurfaceLost() override {}
  void DisplaySetMemoryPolicy(const ManagedMemoryPolicy& policy) override {}
};

void CopyCallback(bool* called, std::unique_ptr<CopyOutputResult> result) {
  *called = true;
}

// Check that frame is damaged and swapped only under correct conditions.
TEST_F(DisplayTest, DisplayDamaged) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  settings.finish_rendering_on_resize = true;
  SetUpDisplay(settings, nullptr);

  StubDisplayClient client;
  display_->Initialize(&client);

  SurfaceId surface_id(id_allocator_.GenerateId());
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->has_new_root_surface);
  display_->SetSurfaceId(surface_id, 1.f);
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->display_resized_);
  EXPECT_TRUE(scheduler_->has_new_root_surface);

  scheduler_->ResetDamageForTest();
  display_->Resize(gfx::Size(100, 100));
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_TRUE(scheduler_->display_resized_);
  EXPECT_FALSE(scheduler_->has_new_root_surface);

  factory_.Create(surface_id);

  // First draw from surface should have full damage.
  RenderPassList pass_list;
  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = RenderPassId(1, 1);
  pass_list.push_back(std::move(pass));

  scheduler_->ResetDamageForTest();
  SubmitCompositorFrame(&pass_list, surface_id);
  EXPECT_TRUE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->display_resized_);
  EXPECT_FALSE(scheduler_->has_new_root_surface);

  EXPECT_FALSE(scheduler_->swapped);
  EXPECT_EQ(0u, output_surface_->num_sent_frames());
  display_->DrawAndSwap();
  EXPECT_TRUE(scheduler_->swapped);
  EXPECT_EQ(1u, output_surface_->num_sent_frames());
  EXPECT_EQ(gfx::Size(100, 100),
            software_output_device_->viewport_pixel_size());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), software_output_device_->damage_rect());

  {
    // Only damaged portion should be swapped.
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(10, 10, 1, 1), software_output_device_->damage_rect());
  }

  {
    // Pass has no damage so shouldn't be swapped.
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
  }

  {
    // Pass is wrong size so shouldn't be swapped.
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 99, 99);
    pass->damage_rect = gfx::Rect(10, 10, 10, 10);
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
  }

  {
    // Previous frame wasn't swapped, so next swap should have full damage.
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(3u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());
  }

  {
    // Pass has copy output request so should be swapped.
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    bool copy_called = false;
    pass->copy_requests.push_back(CopyOutputRequest::CreateRequest(
        base::Bind(&CopyCallback, &copy_called)));
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
    EXPECT_TRUE(copy_called);
  }

  // Pass has no damage, so shouldn't be swapped, but latency info should be
  // saved for next swap.
  {
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    pass_list.swap(frame_data->render_pass_list);

    CompositorFrame frame;
    frame.delegated_frame_data = std::move(frame_data);
    frame.metadata.latency_info.push_back(ui::LatencyInfo());

    factory_.SubmitCompositorFrame(surface_id, std::move(frame),
                                   SurfaceFactory::DrawCallback());
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
  }

  // Resize should cause a swap if no frame was swapped at the previous size.
  {
    scheduler_->swapped = false;
    display_->Resize(gfx::Size(200, 200));
    EXPECT_FALSE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());

    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 200, 200);
    pass->damage_rect = gfx::Rect(10, 10, 10, 10);
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    pass_list.swap(frame_data->render_pass_list);

    CompositorFrame frame;
    frame.delegated_frame_data = std::move(frame_data);

    factory_.SubmitCompositorFrame(surface_id, std::move(frame),
                                   SurfaceFactory::DrawCallback());
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->Resize(gfx::Size(100, 100));
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(5u, output_surface_->num_sent_frames());

    // Latency info from previous frame should be sent now.
    EXPECT_EQ(1u,
              output_surface_->last_sent_frame()->metadata.latency_info.size());
  }

  {
    // Surface that's damaged completely should be resized and swapped.
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 99, 99);
    pass->damage_rect = gfx::Rect(0, 0, 99, 99);
    pass->id = RenderPassId(1, 1);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(6u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());
    EXPECT_EQ(0u,
              output_surface_->last_sent_frame()->metadata.latency_info.size());
  }

  factory_.Destroy(surface_id);
}

class MockedContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD0(shallowFinishCHROMIUM, void());
};

TEST_F(DisplayTest, Finish) {
  SurfaceId surface_id(id_allocator_.GenerateId());

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  settings.finish_rendering_on_resize = true;

  std::unique_ptr<MockedContext> context(new MockedContext());
  MockedContext* context_ptr = context.get();
  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM()).Times(0);

  SetUpDisplay(settings, std::move(context));

  StubDisplayClient client;
  display_->Initialize(&client);

  display_->SetSurfaceId(surface_id, 1.f);

  display_->Resize(gfx::Size(100, 100));
  factory_.Create(surface_id);

  {
    RenderPassList pass_list;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = RenderPassId(1, 1);
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, surface_id);
  }

  display_->DrawAndSwap();

  // First resize and draw shouldn't finish.
  testing::Mock::VerifyAndClearExpectations(context_ptr);

  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM());
  display_->Resize(gfx::Size(150, 150));
  testing::Mock::VerifyAndClearExpectations(context_ptr);

  // Another resize without a swap doesn't need to finish.
  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM()).Times(0);
  display_->Resize(gfx::Size(200, 200));
  testing::Mock::VerifyAndClearExpectations(context_ptr);

  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM()).Times(0);
  {
    RenderPassList pass_list;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 200, 200);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = RenderPassId(1, 1);
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, surface_id);
  }

  display_->DrawAndSwap();

  testing::Mock::VerifyAndClearExpectations(context_ptr);

  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM());
  display_->Resize(gfx::Size(250, 250));
  testing::Mock::VerifyAndClearExpectations(context_ptr);

  factory_.Destroy(surface_id);
}

}  // namespace
}  // namespace cc
