// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/output/output_surface_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/context_factories_for_test.h"

#if defined(USE_OZONE)
#include "components/display_compositor/compositor_overlay_candidate_validator_ozone.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#endif  // defined(USE_OZONE)

namespace content {
namespace {
class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  FakeTaskRunner() {}

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    return true;
  }
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    return true;
  }
  bool RunsTasksOnCurrentThread() const override { return true; }

 protected:
  ~FakeTaskRunner() override {}
};

#if defined(USE_OZONE)
class TestOverlayCandidatesOzone : public ui::OverlayCandidatesOzone {
 public:
  TestOverlayCandidatesOzone() {}
  ~TestOverlayCandidatesOzone() override {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces) override {
    (*surfaces)[0].overlay_handled = true;
  }
};
#endif  // defined(USE_OZONE)

std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
CreateTestValidatorOzone() {
#if defined(USE_OZONE)
  return std::unique_ptr<
      display_compositor::CompositorOverlayCandidateValidator>(
      new display_compositor::CompositorOverlayCandidateValidatorOzone(
          std::unique_ptr<ui::OverlayCandidatesOzone>(
              new TestOverlayCandidatesOzone()),
          false));
#else
  return nullptr;
#endif  // defined(USE_OZONE)
}

class TestOutputSurface : public BrowserCompositorOutputSurface {
 public:
  TestOutputSurface(scoped_refptr<cc::ContextProvider> context_provider,
                    scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
                    cc::SyntheticBeginFrameSource* begin_frame_source)
      : BrowserCompositorOutputSurface(std::move(context_provider),
                                       std::move(vsync_manager),
                                       begin_frame_source,
                                       CreateTestValidatorOzone()) {
  }

  void SetFlip(bool flip) { capabilities_.flipped_output_surface = flip; }

  void BindToClient(cc::OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {}
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override {}
  void SwapBuffers(cc::OutputSurfaceFrame frame) override {}
  uint32_t GetFramebufferCopyTextureFormat() override { return GL_RGB; }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  bool SurfaceIsSuspendForRecycle() const override { return false; }

  void OnReflectorChanged() override {
    if (!reflector_) {
      reflector_texture_.reset();
    } else {
      reflector_texture_.reset(new ReflectorTexture(context_provider()));
      reflector_->OnSourceTextureMailboxUpdated(reflector_texture_->mailbox());
    }
  }

#if defined(OS_MACOSX)
  void SetSurfaceSuspendedForRecycle(bool suspended) override {}
#endif

 private:
  std::unique_ptr<ReflectorTexture> reflector_texture_;
};

const gfx::Rect kSubRect(0, 0, 64, 64);
const gfx::Size kSurfaceSize(256, 256);

}  // namespace

class ReflectorImplTest : public testing::Test {
 public:
  void SetUp() override {
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);
    ImageTransportFactory::InitializeForUnitTests(
        std::unique_ptr<ImageTransportFactory>(
            new NoTransportImageTransportFactory));
    message_loop_.reset(new base::MessageLoop());
    task_runner_ = message_loop_->task_runner();
    compositor_task_runner_ = new FakeTaskRunner();
    begin_frame_source_.reset(new cc::DelayBasedBeginFrameSource(
        base::MakeUnique<cc::DelayBasedTimeSource>(
            compositor_task_runner_.get())));
    compositor_.reset(
        new ui::Compositor(context_factory, compositor_task_runner_.get()));
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);

    auto context_provider = cc::TestContextProvider::Create();
    context_provider->BindToCurrentThread();
    output_surface_ = base::MakeUnique<TestOutputSurface>(
        std::move(context_provider), compositor_->vsync_manager(),
        begin_frame_source_.get());

    root_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    compositor_->SetRootLayer(root_layer_.get());
    mirroring_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    compositor_->root_layer()->Add(mirroring_layer_.get());
    output_surface_->Reshape(kSurfaceSize, 1.f, gfx::ColorSpace(), false);
    mirroring_layer_->SetBounds(gfx::Rect(kSurfaceSize));
  }

  void SetUpReflector() {
    reflector_ = base::MakeUnique<ReflectorImpl>(compositor_.get(),
                                                 mirroring_layer_.get());
    reflector_->OnSourceSurfaceReady(output_surface_.get());
  }

  void TearDown() override {
    if (reflector_)
      reflector_->RemoveMirroringLayer(mirroring_layer_.get());
    cc::TextureMailbox mailbox;
    std::unique_ptr<cc::SingleReleaseCallback> release;
    if (mirroring_layer_->PrepareTextureMailbox(&mailbox, &release)) {
      release->Run(gpu::SyncToken(), false);
    }
    compositor_.reset();
    ui::TerminateContextFactoryForTests();
    ImageTransportFactory::Terminate();
  }

  void UpdateTexture() {
    reflector_->OnSourcePostSubBuffer(kSubRect, kSurfaceSize);
  }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<ui::Compositor> compositor_;
  std::unique_ptr<ui::Layer> root_layer_;
  std::unique_ptr<ui::Layer> mirroring_layer_;
  std::unique_ptr<ReflectorImpl> reflector_;
  std::unique_ptr<TestOutputSurface> output_surface_;
};

namespace {
TEST_F(ReflectorImplTest, CheckNormalOutputSurface) {
  output_surface_->SetFlip(false);
  SetUpReflector();
  UpdateTexture();
  EXPECT_TRUE(mirroring_layer_->TextureFlipped());
  gfx::Rect expected_rect = kSubRect + gfx::Vector2d(0, kSurfaceSize.height()) -
                            gfx::Vector2d(0, kSubRect.height());
  EXPECT_EQ(expected_rect, mirroring_layer_->damaged_region());
}

TEST_F(ReflectorImplTest, CheckInvertedOutputSurface) {
  output_surface_->SetFlip(true);
  SetUpReflector();
  UpdateTexture();
  EXPECT_FALSE(mirroring_layer_->TextureFlipped());
  EXPECT_EQ(kSubRect, mirroring_layer_->damaged_region());
}

#if defined(USE_OZONE)
TEST_F(ReflectorImplTest, CheckOverlayNoReflector) {
  cc::OverlayCandidateList list;
  cc::OverlayCandidate plane_1, plane_2;
  plane_1.plane_z_order = 0;
  plane_2.plane_z_order = 1;
  list.push_back(plane_1);
  list.push_back(plane_2);
  output_surface_->GetOverlayCandidateValidator()->CheckOverlaySupport(&list);
  EXPECT_TRUE(list[0].overlay_handled);
}

TEST_F(ReflectorImplTest, CheckOverlaySWMirroring) {
  SetUpReflector();
  cc::OverlayCandidateList list;
  cc::OverlayCandidate plane_1, plane_2;
  plane_1.plane_z_order = 0;
  plane_2.plane_z_order = 1;
  list.push_back(plane_1);
  list.push_back(plane_2);
  output_surface_->GetOverlayCandidateValidator()->CheckOverlaySupport(&list);
  EXPECT_FALSE(list[0].overlay_handled);
}
#endif  // defined(USE_OZONE)

}  // namespace
}  // namespace content
