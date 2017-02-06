// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/base/region.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/overlay_processor.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"

using testing::_;
using testing::Mock;

namespace cc {
namespace {

const gfx::Size kDisplaySize(256, 256);
const gfx::Rect kOverlayRect(0, 0, 128, 128);
const gfx::Rect kOverlayTopLeftRect(0, 0, 64, 64);
const gfx::Rect kOverlayBottomRightRect(64, 64, 64, 64);
const gfx::Rect kOverlayClipRect(0, 0, 128, 128);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);
const gfx::Transform kNormalTransform =
    gfx::Transform(0.9f, 0, 0, 0.8f, 0.1f, 0.2f);  // x,y -> x,y.
const gfx::Transform kXMirrorTransform =
    gfx::Transform(-0.9f, 0, 0, 0.8f, 1.0f, 0.2f);  // x,y -> 1-x,y.
const gfx::Transform kYMirrorTransform =
    gfx::Transform(0.9f, 0, 0, -0.8f, 0.1f, 1.0f);  // x,y -> x,1-y.
const gfx::Transform kBothMirrorTransform =
    gfx::Transform(-0.9f, 0, 0, -0.8f, 1.0f, 1.0f);  // x,y -> 1-x,1-y.
const gfx::Transform kSwapTransform =
    gfx::Transform(0, 1, 1, 0, 0, 0);  // x,y -> y,x.

void MailboxReleased(const gpu::SyncToken& sync_token,
                     bool lost_resource,
                     BlockingTaskRunner* main_thread_task_runner) {}

class SingleOverlayValidator : public OverlayCandidateValidator {
 public:
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override {
    strategies->push_back(
        base::WrapUnique(new OverlayStrategySingleOnTop(this)));
    strategies->push_back(base::WrapUnique(new OverlayStrategyUnderlay(this)));
  }
  bool AllowCALayerOverlays() override { return false; }
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override {
    // We may have 1 or 2 surfaces depending on whether this ran through the
    // full renderer and picked up the output surface, or not.
    ASSERT_LE(1U, surfaces->size());
    ASSERT_GE(2U, surfaces->size());

    OverlayCandidate& candidate = surfaces->back();
    EXPECT_TRUE(!candidate.use_output_surface_for_resource);
    if (candidate.display_rect.width() == 64) {
      EXPECT_EQ(gfx::RectF(kOverlayBottomRightRect), candidate.display_rect);
    } else {
      EXPECT_NEAR(kOverlayRect.x(), candidate.display_rect.x(), 0.01f);
      EXPECT_NEAR(kOverlayRect.y(), candidate.display_rect.y(), 0.01f);
      EXPECT_NEAR(kOverlayRect.width(), candidate.display_rect.width(), 0.01f);
      EXPECT_NEAR(kOverlayRect.height(), candidate.display_rect.height(),
                  0.01f);
    }
    EXPECT_FLOAT_RECT_EQ(BoundingRect(kUVTopLeft, kUVBottomRight),
                         candidate.uv_rect);
    if (!candidate.clip_rect.IsEmpty()) {
      EXPECT_EQ(true, candidate.is_clipped);
      EXPECT_EQ(kOverlayClipRect, candidate.clip_rect);
    }
    candidate.overlay_handled = true;
  }
};

class CALayerValidator : public OverlayCandidateValidator {
 public:
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override {}
  bool AllowCALayerOverlays() override { return true; }
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override {}
};

class SingleOnTopOverlayValidator : public SingleOverlayValidator {
 public:
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override {
    strategies->push_back(
        base::WrapUnique(new OverlayStrategySingleOnTop(this)));
  }
};

class UnderlayOverlayValidator : public SingleOverlayValidator {
 public:
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override {
    strategies->push_back(base::WrapUnique(new OverlayStrategyUnderlay(this)));
  }
};

class DefaultOverlayProcessor : public OverlayProcessor {
 public:
  explicit DefaultOverlayProcessor(OutputSurface* surface);
  size_t GetStrategyCount();
};

DefaultOverlayProcessor::DefaultOverlayProcessor(OutputSurface* surface)
    : OverlayProcessor(surface) {
}

size_t DefaultOverlayProcessor::GetStrategyCount() {
  return strategies_.size();
}

class OverlayOutputSurface : public OutputSurface {
 public:
  explicit OverlayOutputSurface(
      scoped_refptr<TestContextProvider> context_provider)
      : OutputSurface(std::move(context_provider), nullptr, nullptr) {
    surface_size_ = kDisplaySize;
    device_scale_factor_ = 1;
    is_displayed_as_overlay_plane_ = true;
  }

  void SetScaleFactor(float scale_factor) {
    device_scale_factor_ = scale_factor;
  }

  // OutputSurface implementation
  void BindFramebuffer() override {
    OutputSurface::BindFramebuffer();
    bind_framebuffer_count_ += 1;
  }
  uint32_t GetFramebufferCopyTextureFormat() override {
    // TestContextProvider has no real framebuffer, just use RGB.
    return GL_RGB;
  }
  void SwapBuffers(CompositorFrame frame) override {
    client_->DidSwapBuffers();
  }
  void OnSwapBuffersComplete() override { client_->DidSwapBuffersComplete(); }

  void SetOverlayCandidateValidator(OverlayCandidateValidator* validator) {
    overlay_candidate_validator_.reset(validator);
  }

  OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
    return overlay_candidate_validator_.get();
  }

  bool IsDisplayedAsOverlayPlane() const override {
    return is_displayed_as_overlay_plane_;
  }
  unsigned GetOverlayTextureId() const override { return 10000; }
  void set_is_displayed_as_overlay_plane(bool value) {
    is_displayed_as_overlay_plane_ = value;
  }
  unsigned bind_framebuffer_count() const { return bind_framebuffer_count_; }

 private:
  std::unique_ptr<OverlayCandidateValidator> overlay_candidate_validator_;
  bool is_displayed_as_overlay_plane_;
  unsigned bind_framebuffer_count_ = 0;
};

std::unique_ptr<RenderPass> CreateRenderPass() {
  RenderPassId id(1, 0);
  gfx::Rect output_rect(0, 0, 256, 256);
  bool has_transparent_background = true;

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetAll(id,
               output_rect,
               output_rect,
               gfx::Transform(),
               has_transparent_background);

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  return pass;
}

ResourceId CreateResource(ResourceProvider* resource_provider,
                          const gfx::Size& size,
                          bool is_overlay_candidate) {
  TextureMailbox mailbox =
      TextureMailbox(gpu::Mailbox::Generate(), gpu::SyncToken(), GL_TEXTURE_2D,
                     size, is_overlay_candidate, false);
  std::unique_ptr<SingleReleaseCallbackImpl> release_callback =
      SingleReleaseCallbackImpl::Create(base::Bind(&MailboxReleased));

  return resource_provider->CreateResourceFromTextureMailbox(
      mailbox, std::move(release_callback));
}

SolidColorDrawQuad* CreateSolidColorQuadAt(
    const SharedQuadState* shared_quad_state,
    SkColor color,
    RenderPass* render_pass,
    const gfx::Rect& rect) {
  SolidColorDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, rect, rect, color, false);
  return quad;
}

TextureDrawQuad* CreateCandidateQuadAt(ResourceProvider* resource_provider,
                                       const SharedQuadState* shared_quad_state,
                                       RenderPass* render_pass,
                                       const gfx::Rect& rect) {
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::Size resource_size_in_pixels = gfx::Size(64, 64);
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      resource_provider, resource_size_in_pixels, is_overlay_candidate);

  TextureDrawQuad* overlay_quad =
      render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, rect, resource_id,
                       premultiplied_alpha, kUVTopLeft, kUVBottomRight,
                       SK_ColorTRANSPARENT, vertex_opacity, flipped,
                       nearest_neighbor, false);
  overlay_quad->set_resource_size_in_pixels(resource_size_in_pixels);

  return overlay_quad;
}

StreamVideoDrawQuad* CreateCandidateVideoQuadAt(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass,
    const gfx::Rect& rect,
    const gfx::Transform& transform) {
  gfx::Size resource_size_in_pixels = gfx::Size(64, 64);
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      resource_provider, resource_size_in_pixels, is_overlay_candidate);

  StreamVideoDrawQuad* overlay_quad =
      render_pass->CreateAndAppendDrawQuad<StreamVideoDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, rect, resource_id,
                       resource_size_in_pixels, transform);

  return overlay_quad;
}

TextureDrawQuad* CreateFullscreenCandidateQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass) {
  return CreateCandidateQuadAt(
      resource_provider, shared_quad_state, render_pass, kOverlayRect);
}

StreamVideoDrawQuad* CreateFullscreenCandidateVideoQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass,
    const gfx::Transform& transform) {
  return CreateCandidateVideoQuadAt(resource_provider, shared_quad_state,
                                    render_pass, kOverlayRect, transform);
}

void CreateOpaqueQuadAt(ResourceProvider* resource_provider,
                        const SharedQuadState* shared_quad_state,
                        RenderPass* render_pass,
                        const gfx::Rect& rect) {
  SolidColorDrawQuad* color_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
}

void CreateFullscreenOpaqueQuad(ResourceProvider* resource_provider,
                                const SharedQuadState* shared_quad_state,
                                RenderPass* render_pass) {
  CreateOpaqueQuadAt(resource_provider, shared_quad_state, render_pass,
                     kOverlayRect);
}

static void CompareRenderPassLists(const RenderPassList& expected_list,
                                   const RenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    RenderPass* expected = expected_list[i].get();
    RenderPass* actual = actual_list[i].get();

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);

    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (auto exp_iter = expected->quad_list.cbegin(),
              act_iter = actual->quad_list.cbegin();
         exp_iter != expected->quad_list.cend();
         ++exp_iter, ++act_iter) {
      EXPECT_EQ(exp_iter->rect.ToString(), act_iter->rect.ToString());
      EXPECT_EQ(exp_iter->shared_quad_state->quad_layer_bounds.ToString(),
                act_iter->shared_quad_state->quad_layer_bounds.ToString());
    }
  }
}

template <typename OverlayCandidateValidatorType>
class OverlayTest : public testing::Test {
 protected:
  void SetUp() override {
    provider_ = TestContextProvider::Create();
    output_surface_.reset(new OverlayOutputSurface(provider_));
    EXPECT_TRUE(output_surface_->BindToClient(&client_));
    output_surface_->SetOverlayCandidateValidator(
        new OverlayCandidateValidatorType);

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get());

    overlay_processor_.reset(new OverlayProcessor(output_surface_.get()));
    overlay_processor_->Initialize();
  }

  scoped_refptr<TestContextProvider> provider_;
  std::unique_ptr<OverlayOutputSurface> output_surface_;
  FakeOutputSurfaceClient client_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<OverlayProcessor> overlay_processor_;
  gfx::Rect damage_rect_;
};

typedef OverlayTest<SingleOnTopOverlayValidator> SingleOverlayOnTopTest;
typedef OverlayTest<UnderlayOverlayValidator> UnderlayTest;
typedef OverlayTest<CALayerValidator> CALayerOverlayTest;

TEST(OverlayTest, NoOverlaysByDefault) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  EXPECT_EQ(NULL, output_surface.GetOverlayCandidateValidator());

  output_surface.SetOverlayCandidateValidator(new SingleOverlayValidator);
  EXPECT_TRUE(output_surface.GetOverlayCandidateValidator() != NULL);
}

TEST(OverlayTest, OverlaysProcessorHasStrategy) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  output_surface.SetOverlayCandidateValidator(new SingleOverlayValidator);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(&output_surface,
                                   shared_bitmap_manager.get());

  std::unique_ptr<DefaultOverlayProcessor> overlay_processor(
      new DefaultOverlayProcessor(&output_surface));
  overlay_processor->Initialize();
  EXPECT_GE(2U, overlay_processor->GetStrategyCount());
}

TEST_F(SingleOverlayOnTopTest, SuccessfulOverlay) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* original_quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  unsigned original_resource_id = original_quad->resource_id();

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());

  RenderPass* main_pass = pass.get();
  // Check that the quad is gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
  const QuadList& quad_list = main_pass->quad_list;
  for (QuadList::ConstBackToFrontIterator it = quad_list.BackToFrontBegin();
       it != quad_list.BackToFrontEnd();
       ++it) {
    EXPECT_NE(DrawQuad::TEXTURE_CONTENT, it->material);
  }

  // Check that the right resource id got extracted.
  EXPECT_EQ(original_resource_id, candidate_list.back().resource_id);
}

TEST_F(SingleOverlayOnTopTest, DamageRect) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  damage_rect_ = kOverlayRect;

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;

  // Primary plane.
  OverlayCandidate output_surface_plane;
  output_surface_plane.display_rect = gfx::RectF(kOverlayRect);
  output_surface_plane.quad_rect_in_target_space = kOverlayRect;
  output_surface_plane.use_output_surface_for_resource = true;
  output_surface_plane.overlay_handled = true;
  candidate_list.push_back(output_surface_plane);

  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  DCHECK(damage_rect_.IsEmpty());
}

TEST_F(SingleOverlayOnTopTest, NoCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), pass_list.back().get(), &candidate_list,
      nullptr, &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

TEST_F(SingleOverlayOnTopTest, OccludedCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), pass_list.back().get(), &candidate_list,
      nullptr, &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

// Test with multiple render passes.
TEST_F(SingleOverlayOnTopTest, MultipleRenderPasses) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectPremultipliedAlpha) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  quad->premultiplied_alpha = true;

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlending) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  quad->needs_blending = true;

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBackgroundColor) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  quad->background_color = SK_ColorBLACK;

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlendMode) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->blend_mode = SkXfermode::kScreen_Mode;

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpacity) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->opacity = 0.5f;

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectNonAxisAlignedTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutXAxis(45.f);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowClipped) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = kOverlayClipRect;

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVerticalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  rect.Offset(0, -rect.height());
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      -1.0f);
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL,
            candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowHorizontalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_height(rect.height() / 2);
  rect.Offset(-rect.width(), 0);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(-1.0f,
                                                                      2.0f);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL,
            candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowPositiveScaleTransform) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      1.0f);
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, Allow90DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(0, -rect.height());
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(90.f);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_90, candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, Allow180DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), -rect.height());
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(180.f);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_180, candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, Allow270DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), 0);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(270.f);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_270, candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowNotTopIfNotOccluded) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(),
                        pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentOnTop) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 0.f;
  CreateSolidColorQuadAt(shared_state, SK_ColorBLACK, pass.get(),
                         kOverlayBottomRightRect);
  shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateCandidateQuadAt(resource_provider_.get(), shared_state, pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentColorOnTop) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateSolidColorQuadAt(pass->shared_quad_state_list.back(),
                         SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpaqueColorOnTop) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 0.5f;
  CreateSolidColorQuadAt(shared_state, SK_ColorBLACK, pass.get(),
                         kOverlayBottomRightRect);
  shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateCandidateQuadAt(resource_provider_.get(), shared_state, pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectTransparentColorOnTopWithoutBlending) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  CreateSolidColorQuadAt(shared_state, SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect)->opaque_rect =
      kOverlayBottomRightRect;
  CreateCandidateQuadAt(resource_provider_.get(), shared_state, pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectVideoSwapTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kSwapTransform);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoXMirrorTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kXMirrorTransform);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoBothMirrorTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kBothMirrorTransform);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoNormalTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kNormalTransform);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoYMirrorTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kYMirrorTransform);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(UnderlayTest, OverlayLayerUnderMainLayer) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(-1, candidate_list[0].plane_z_order);
  EXPECT_EQ(2U, pass->quad_list.size());
  // The overlay quad should have changed to a SOLID_COLOR quad.
  EXPECT_EQ(pass->quad_list.back()->material, DrawQuad::SOLID_COLOR);
}

TEST_F(UnderlayTest, AllowOnTop) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->CreateAndAppendSharedQuadState()->opacity = 0.5f;
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(-1, candidate_list[0].plane_z_order);
  // The overlay quad should have changed to a SOLID_COLOR quad.
  EXPECT_EQ(pass->quad_list.front()->material, DrawQuad::SOLID_COLOR);
}

// The first time an underlay is scheduled its damage must not be subtracted.
TEST_F(UnderlayTest, InitialUnderlayDamageNotSubtracted) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  damage_rect_ = kOverlayRect;

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &candidate_list, nullptr,
                                         &damage_rect_);

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

// An identical underlay for two frames in a row means the damage can be
// subtracted the second time.
TEST_F(UnderlayTest, DamageSubtractedForConsecutiveIdenticalUnderlays) {
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    CreateFullscreenCandidateQuad(resource_provider_.get(),
                                  pass->shared_quad_state_list.back(),
                                  pass.get());

    damage_rect_ = kOverlayRect;

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    OverlayCandidateList candidate_list;
    overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                           &candidate_list, nullptr,
                                           &damage_rect_);
  }

  // The second time the same overlay rect is scheduled it will be subtracted
  // from the damage rect.
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

// Underlay damage can only be subtracted if the previous frame's underlay
// was the same rect.
TEST_F(UnderlayTest, DamageNotSubtractedForNonIdenticalConsecutiveUnderlays) {
  gfx::Rect overlay_rects[] = {kOverlayBottomRightRect, kOverlayRect};
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();

    CreateCandidateQuadAt(resource_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          overlay_rects[i]);

    damage_rect_ = overlay_rects[i];

    OverlayCandidateList candidate_list;
    overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                           &candidate_list, nullptr,
                                           &damage_rect_);

    EXPECT_EQ(overlay_rects[i], damage_rect_);
  }
}

TEST_F(UnderlayTest, DamageNotSubtractedWhenQuadsAboveOverlap) {
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    // Add an overlapping quad above the candidate.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());
    CreateFullscreenCandidateQuad(resource_provider_.get(),
                                  pass->shared_quad_state_list.back(),
                                  pass.get());

    damage_rect_ = kOverlayRect;

    OverlayCandidateList candidate_list;
    overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                           &candidate_list, nullptr,
                                           &damage_rect_);
  }

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

TEST_F(UnderlayTest, DamageSubtractedWhenQuadsAboveDontOverlap) {
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    // Add a non-overlapping quad above the candidate.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       kOverlayTopLeftRect);
    CreateCandidateQuadAt(resource_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          kOverlayBottomRightRect);

    damage_rect_ = kOverlayBottomRightRect;

    OverlayCandidateList candidate_list;
    overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                           &candidate_list, nullptr,
                                           &damage_rect_);
  }

  EXPECT_TRUE(damage_rect_.IsEmpty());
}

OverlayCandidateList BackbufferOverlayList(const RenderPass* root_render_pass) {
  OverlayCandidateList list;
  OverlayCandidate output_surface_plane;
  output_surface_plane.display_rect = gfx::RectF(root_render_pass->output_rect);
  output_surface_plane.quad_rect_in_target_space =
      root_render_pass->output_rect;
  output_surface_plane.use_output_surface_for_resource = true;
  output_surface_plane.overlay_handled = true;
  list.push_back(output_surface_plane);
  return list;
}

TEST_F(CALayerOverlayTest, AllowNonAxisAlignedTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(45.f);

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayCandidateList overlay_list(BackbufferOverlayList(pass.get()));
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &overlay_list, &ca_layer_list,
                                         &damage_rect);
  EXPECT_EQ(0U, pass->quad_list.size());
  EXPECT_EQ(0U, overlay_list.size());
  EXPECT_EQ(1U, ca_layer_list.size());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, ThreeDTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutXAxis(45.f);

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayCandidateList overlay_list(BackbufferOverlayList(pass.get()));
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &overlay_list, &ca_layer_list,
                                         &damage_rect);
  EXPECT_EQ(0U, overlay_list.size());
  EXPECT_EQ(1U, ca_layer_list.size());
  gfx::Transform expected_transform;
  expected_transform.RotateAboutXAxis(45.f);
  gfx::Transform actual_transform(ca_layer_list.back().transform);
  EXPECT_EQ(expected_transform.ToString(), actual_transform.ToString());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, AllowContainingClip) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = kOverlayRect;

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayCandidateList overlay_list(BackbufferOverlayList(pass.get()));
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &overlay_list, &ca_layer_list,
                                         &damage_rect);
  EXPECT_EQ(0U, pass->quad_list.size());
  EXPECT_EQ(0U, overlay_list.size());
  EXPECT_EQ(1U, ca_layer_list.size());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, NontrivialClip) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = gfx::Rect(64, 64, 128, 128);

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayCandidateList overlay_list(BackbufferOverlayList(pass.get()));
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &overlay_list, &ca_layer_list,
                                         &damage_rect);
  EXPECT_EQ(0U, pass->quad_list.size());
  EXPECT_EQ(0U, overlay_list.size());
  EXPECT_EQ(1U, ca_layer_list.size());
  EXPECT_TRUE(ca_layer_list.back().is_clipped);
  EXPECT_EQ(gfx::RectF(64, 64, 128, 128), ca_layer_list.back().clip_rect);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, SkipTransparent) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->opacity = 0;

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayCandidateList overlay_list(BackbufferOverlayList(pass.get()));
  overlay_processor_->ProcessForOverlays(resource_provider_.get(), pass.get(),
                                         &overlay_list, &ca_layer_list,
                                         &damage_rect);
  EXPECT_EQ(0U, pass->quad_list.size());
  EXPECT_EQ(0U, overlay_list.size());
  EXPECT_EQ(0U, ca_layer_list.size());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

class OverlayInfoRendererGL : public GLRenderer {
 public:
  OverlayInfoRendererGL(RendererClient* client,
                        const RendererSettings* settings,
                        OutputSurface* output_surface,
                        ResourceProvider* resource_provider)
      : GLRenderer(client,
                   settings,
                   output_surface,
                   resource_provider,
                   NULL,
                   0),
        expect_overlays_(false) {}

  MOCK_METHOD3(DoDrawQuad,
               void(DrawingFrame* frame,
                    const DrawQuad* quad,
                    const gfx::QuadF* draw_region));

  using GLRenderer::BeginDrawingFrame;

  void FinishDrawingFrame(DrawingFrame* frame) override {
    GLRenderer::FinishDrawingFrame(frame);

    if (!expect_overlays_) {
      EXPECT_EQ(0U, frame->overlay_list.size());
      return;
    }

    ASSERT_EQ(2U, frame->overlay_list.size());
    EXPECT_GE(frame->overlay_list.back().resource_id, 0U);
  }

  void set_expect_overlays(bool expect_overlays) {
    expect_overlays_ = expect_overlays;
  }

 private:
  bool expect_overlays_;
};

class FakeRendererClient : public RendererClient {
 public:
  // RendererClient methods.
  void SetFullRootLayerDamage() override {}
};

class MockOverlayScheduler {
 public:
  MOCK_METHOD5(Schedule,
               void(int plane_z_order,
                    gfx::OverlayTransform plane_transform,
                    unsigned overlay_texture_id,
                    const gfx::Rect& display_bounds,
                    const gfx::RectF& uv_rect));
};

class GLRendererWithOverlaysTest : public testing::Test {
 protected:
  GLRendererWithOverlaysTest() {
    provider_ = TestContextProvider::Create();
    output_surface_.reset(new OverlayOutputSurface(provider_));
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ =
        FakeResourceProvider::Create(output_surface_.get(), nullptr);

    provider_->support()->SetScheduleOverlayPlaneCallback(base::Bind(
        &MockOverlayScheduler::Schedule, base::Unretained(&scheduler_)));
  }

  void Init(bool use_validator) {
    if (use_validator)
      output_surface_->SetOverlayCandidateValidator(new SingleOverlayValidator);

    renderer_ = base::WrapUnique(new OverlayInfoRendererGL(
        &renderer_client_, &settings_, output_surface_.get(),
        resource_provider_.get()));
  }

  void DrawFrame(RenderPassList* pass_list, const gfx::Rect& viewport_rect) {
    renderer_->DrawFrame(pass_list, 1.f, gfx::ColorSpace(), viewport_rect,
                         viewport_rect, false);
  }
  void SwapBuffers() {
    renderer_->SwapBuffers(CompositorFrameMetadata());
    output_surface_->OnSwapBuffersComplete();
    renderer_->SwapBuffersComplete();
  }
  void SwapBuffersWithoutComplete() {
    renderer_->SwapBuffers(CompositorFrameMetadata());
  }
  void SwapBuffersComplete() {
    output_surface_->OnSwapBuffersComplete();
    renderer_->SwapBuffersComplete();
  }
  void ReturnResourceInUseQuery(ResourceId id) {
    ResourceProvider::ScopedReadLockGL lock(resource_provider_.get(), id);
    gpu::TextureInUseResponse response;
    response.texture = lock.texture_id();
    response.in_use = false;
    gpu::TextureInUseResponses responses;
    responses.push_back(response);
    renderer_->DidReceiveTextureInUseResponses(responses);
  }

  RendererSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<OverlayOutputSurface> output_surface_;
  FakeRendererClient renderer_client_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<OverlayInfoRendererGL> renderer_;
  scoped_refptr<TestContextProvider> provider_;
  MockOverlayScheduler scheduler_;
};

TEST_F(GLRendererWithOverlaysTest, OverlayQuadNotDrawn) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  gfx::Rect viewport_rect(16, 16);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Candidate pass was taken out and extra skipped pass added,
  // so only draw 2 quads.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(2);
  EXPECT_CALL(scheduler_,
              Schedule(0, gfx::OVERLAY_TRANSFORM_NONE, _,
                       gfx::Rect(kDisplaySize), gfx::RectF(0, 0, 1, 1)))
      .Times(1);
  EXPECT_CALL(scheduler_, Schedule(1, gfx::OVERLAY_TRANSFORM_NONE, _,
                                   kOverlayBottomRightRect,
                                   BoundingRect(kUVTopLeft, kUVBottomRight)))
      .Times(1);
  DrawFrame(&pass_list, viewport_rect);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, OccludedQuadInUnderlay) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  gfx::Rect viewport_rect(16, 16);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Candidate quad should fail to be overlaid on top because of occlusion.
  // Expect to be replaced with transparent hole quad and placed in underlay.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(3);
  EXPECT_CALL(scheduler_,
              Schedule(0, gfx::OVERLAY_TRANSFORM_NONE, _,
                       gfx::Rect(kDisplaySize), gfx::RectF(0, 0, 1, 1)))
      .Times(1);
  EXPECT_CALL(scheduler_,
              Schedule(-1, gfx::OVERLAY_TRANSFORM_NONE, _, kOverlayRect,
                       BoundingRect(kUVTopLeft, kUVBottomRight)))
      .Times(1);
  DrawFrame(&pass_list, viewport_rect);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, NoValidatorNoOverlay) {
  bool use_validator = false;
  Init(use_validator);
  renderer_->set_expect_overlays(false);
  gfx::Rect viewport_rect(16, 16);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Should not see the primary surface's overlay.
  output_surface_->set_is_displayed_as_overlay_plane(false);
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(3);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  DrawFrame(&pass_list, viewport_rect);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

// GLRenderer skips drawing occluded quads when partial swap is enabled.
TEST_F(GLRendererWithOverlaysTest, OccludedQuadNotDrawnWhenPartialSwapEnabled) {
  provider_->TestContext3d()->set_have_post_sub_buffer(true);
  settings_.partial_swap_enabled = true;
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  gfx::Rect viewport_rect(16, 16);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  output_surface_->set_is_displayed_as_overlay_plane(true);
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(0);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  DrawFrame(&pass_list, viewport_rect);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

// GLRenderer skips drawing occluded quads when empty swap is enabled.
TEST_F(GLRendererWithOverlaysTest, OccludedQuadNotDrawnWhenEmptySwapAllowed) {
  provider_->TestContext3d()->set_have_commit_overlay_planes(true);
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  gfx::Rect viewport_rect(16, 16);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  output_surface_->set_is_displayed_as_overlay_plane(true);
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(0);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  DrawFrame(&pass_list, viewport_rect);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, ResourcesExportedAndReturnedWithDelay) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  ResourceId resource1 =
      CreateResource(resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource2 =
      CreateResource(resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource3 =
      CreateResource(resource_provider_.get(), gfx::Size(32, 32), true);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  DirectRenderer::DrawingFrame frame1;
  frame1.render_passes_in_draw_order = &pass_list;
  frame1.overlay_list.resize(2);
  frame1.overlay_list.front().use_output_surface_for_resource = true;
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.render_passes_in_draw_order = &pass_list;
  frame2.overlay_list.resize(2);
  frame2.overlay_list.front().use_output_surface_for_resource = true;
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = resource2;
  overlay2.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame3;
  frame3.render_passes_in_draw_order = &pass_list;
  frame3.overlay_list.resize(2);
  frame3.overlay_list.front().use_output_surface_for_resource = true;
  OverlayCandidate& overlay3 = frame3.overlay_list.back();
  overlay3.resource_id = resource3;
  overlay3.plane_z_order = 1;

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame2);
  renderer_->FinishDrawingFrame(&frame2);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame3);
  renderer_->FinishDrawingFrame(&frame3);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // No overlays, release the resource.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  DirectRenderer::DrawingFrame frame_no_overlays;
  frame_no_overlays.render_passes_in_draw_order = &pass_list;
  renderer_->set_expect_overlays(false);
  renderer_->BeginDrawingFrame(&frame_no_overlays);
  renderer_->FinishDrawingFrame(&frame_no_overlays);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // Use the same buffer twice.
  renderer_->set_expect_overlays(true);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  renderer_->set_expect_overlays(false);
  renderer_->BeginDrawingFrame(&frame_no_overlays);
  renderer_->FinishDrawingFrame(&frame_no_overlays);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  renderer_->set_expect_overlays(false);
  renderer_->BeginDrawingFrame(&frame_no_overlays);
  renderer_->FinishDrawingFrame(&frame_no_overlays);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, ResourcesExportedAndReturnedAfterGpuQuery) {
  bool use_validator = true;
  settings_.release_overlay_resources_after_gpu_query = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  ResourceId resource1 =
      CreateResource(resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource2 =
      CreateResource(resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource3 =
      CreateResource(resource_provider_.get(), gfx::Size(32, 32), true);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  DirectRenderer::DrawingFrame frame1;
  frame1.render_passes_in_draw_order = &pass_list;
  frame1.overlay_list.resize(2);
  frame1.overlay_list.front().use_output_surface_for_resource = true;
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.render_passes_in_draw_order = &pass_list;
  frame2.overlay_list.resize(2);
  frame2.overlay_list.front().use_output_surface_for_resource = true;
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = resource2;
  overlay2.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame3;
  frame3.render_passes_in_draw_order = &pass_list;
  frame3.overlay_list.resize(2);
  frame3.overlay_list.front().use_output_surface_for_resource = true;
  OverlayCandidate& overlay3 = frame3.overlay_list.back();
  overlay3.resource_id = resource3;
  overlay3.plane_z_order = 1;

  // First frame, with no swap completion.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // Second frame, with no swap completion.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame2);
  renderer_->FinishDrawingFrame(&frame2);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // Third frame, still with no swap completion (where the resources would
  // otherwise have been released).
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(2);
  renderer_->BeginDrawingFrame(&frame3);
  renderer_->FinishDrawingFrame(&frame3);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // This completion corresponds to the first frame.
  SwapBuffersComplete();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));

  // This completion corresponds to the second frame. The first resource is no
  // longer in use.
  ReturnResourceInUseQuery(resource1);
  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));

  // This completion corresponds to the third frame.
  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource3));

  ReturnResourceInUseQuery(resource2);
  ReturnResourceInUseQuery(resource3);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource3));
}

}  // namespace
}  // namespace cc
