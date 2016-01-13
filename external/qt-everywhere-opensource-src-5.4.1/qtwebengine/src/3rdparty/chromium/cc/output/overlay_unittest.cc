// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/overlay_processor.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace cc {
namespace {

const gfx::Rect kOverlayRect(0, 0, 128, 128);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);

void MailboxReleased(unsigned sync_point, bool lost_resource) {}

class SingleOverlayValidator : public OverlayCandidateValidator {
 public:
  virtual void CheckOverlaySupport(OverlayCandidateList* surfaces) OVERRIDE;
};

void SingleOverlayValidator::CheckOverlaySupport(
    OverlayCandidateList* surfaces) {
  ASSERT_EQ(2U, surfaces->size());

  OverlayCandidate& candidate = surfaces->back();
  EXPECT_EQ(kOverlayRect.ToString(), candidate.display_rect.ToString());
  EXPECT_EQ(BoundingRect(kUVTopLeft, kUVBottomRight).ToString(),
            candidate.uv_rect.ToString());
  candidate.overlay_handled = true;
}

class SingleOverlayProcessor : public OverlayProcessor {
 public:
  SingleOverlayProcessor(OutputSurface* surface,
                         ResourceProvider* resource_provider);
  // Virtual to allow testing different strategies.
  virtual void Initialize() OVERRIDE;
};

SingleOverlayProcessor::SingleOverlayProcessor(
    OutputSurface* surface,
    ResourceProvider* resource_provider)
    : OverlayProcessor(surface, resource_provider) {
  EXPECT_EQ(surface, surface_);
  EXPECT_EQ(resource_provider, resource_provider_);
}

void SingleOverlayProcessor::Initialize() {
  OverlayCandidateValidator* candidates =
      surface_->overlay_candidate_validator();
  ASSERT_TRUE(candidates != NULL);
  strategies_.push_back(scoped_ptr<Strategy>(
      new OverlayStrategySingleOnTop(candidates, resource_provider_)));
}

class DefaultOverlayProcessor : public OverlayProcessor {
 public:
  DefaultOverlayProcessor(OutputSurface* surface,
                          ResourceProvider* resource_provider);
  size_t GetStrategyCount();
};

DefaultOverlayProcessor::DefaultOverlayProcessor(
    OutputSurface* surface,
    ResourceProvider* resource_provider)
    : OverlayProcessor(surface, resource_provider) {}

size_t DefaultOverlayProcessor::GetStrategyCount() {
  return strategies_.size();
}

class OverlayOutputSurface : public OutputSurface {
 public:
  explicit OverlayOutputSurface(scoped_refptr<ContextProvider> context_provider)
      : OutputSurface(context_provider) {}

  void InitWithSingleOverlayValidator() {
    overlay_candidate_validator_.reset(new SingleOverlayValidator);
  }
};

scoped_ptr<RenderPass> CreateRenderPass() {
  RenderPass::Id id(1, 0);
  gfx::Rect output_rect(0, 0, 256, 256);
  bool has_transparent_background = true;

  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetAll(id,
               output_rect,
               output_rect,
               gfx::Transform(),
               has_transparent_background);

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  return pass.Pass();
}

ResourceProvider::ResourceId CreateResource(
    ResourceProvider* resource_provider) {
  unsigned sync_point = 0;
  TextureMailbox mailbox =
      TextureMailbox(gpu::Mailbox::Generate(), GL_TEXTURE_2D, sync_point);
  mailbox.set_allow_overlay(true);
  scoped_ptr<SingleReleaseCallback> release_callback =
      SingleReleaseCallback::Create(base::Bind(&MailboxReleased));

  return resource_provider->CreateResourceFromTextureMailbox(
      mailbox, release_callback.Pass());
}

scoped_ptr<TextureDrawQuad> CreateCandidateQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state) {
  ResourceProvider::ResourceId resource_id = CreateResource(resource_provider);
  bool premultiplied_alpha = false;
  bool flipped = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  scoped_ptr<TextureDrawQuad> overlay_quad = TextureDrawQuad::Create();
  overlay_quad->SetNew(shared_quad_state,
                       kOverlayRect,
                       kOverlayRect,
                       kOverlayRect,
                       resource_id,
                       premultiplied_alpha,
                       kUVTopLeft,
                       kUVBottomRight,
                       SK_ColorTRANSPARENT,
                       vertex_opacity,
                       flipped);

  return overlay_quad.Pass();
}

scoped_ptr<DrawQuad> CreateCheckeredQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state) {
  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad =
      CheckerboardDrawQuad::Create();
  checkerboard_quad->SetNew(
      shared_quad_state, kOverlayRect, kOverlayRect, SkColor());
  return checkerboard_quad.PassAs<DrawQuad>();
}

static void CompareRenderPassLists(const RenderPassList& expected_list,
                                   const RenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    RenderPass* expected = expected_list[i];
    RenderPass* actual = actual_list[i];

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_RECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_RECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);

    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (size_t i = 0; i < expected->quad_list.size(); ++i) {
      EXPECT_EQ(expected->quad_list[i]->rect.ToString(),
                actual->quad_list[i]->rect.ToString());
      EXPECT_EQ(
          expected->quad_list[i]->shared_quad_state->content_bounds.ToString(),
          actual->quad_list[i]->shared_quad_state->content_bounds.ToString());
    }
  }
}

TEST(OverlayTest, NoOverlaysByDefault) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  EXPECT_EQ(NULL, output_surface.overlay_candidate_validator());

  output_surface.InitWithSingleOverlayValidator();
  EXPECT_TRUE(output_surface.overlay_candidate_validator() != NULL);
}

TEST(OverlayTest, OverlaysProcessorHasStrategy) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  output_surface.InitWithSingleOverlayValidator();
  EXPECT_TRUE(output_surface.overlay_candidate_validator() != NULL);

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      &output_surface, shared_bitmap_manager.get(), 0, false, 1, false));

  scoped_ptr<DefaultOverlayProcessor> overlay_processor(
      new DefaultOverlayProcessor(&output_surface, resource_provider.get()));
  overlay_processor->Initialize();
  EXPECT_GE(1U, overlay_processor->GetStrategyCount());
}

class SingleOverlayOnTopTest : public testing::Test {
 protected:
  virtual void SetUp() {
    provider_ = TestContextProvider::Create();
    output_surface_.reset(new OverlayOutputSurface(provider_));
    EXPECT_TRUE(output_surface_->BindToClient(&client_));
    output_surface_->InitWithSingleOverlayValidator();
    EXPECT_TRUE(output_surface_->overlay_candidate_validator() != NULL);

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
        false);

    overlay_processor_.reset(new SingleOverlayProcessor(
        output_surface_.get(), resource_provider_.get()));
    overlay_processor_->Initialize();
  }

  scoped_refptr<TestContextProvider> provider_;
  scoped_ptr<OverlayOutputSurface> output_surface_;
  FakeOutputSurfaceClient client_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<SingleOverlayProcessor> overlay_processor_;
};

TEST_F(SingleOverlayOnTopTest, SuccessfullOverlay) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> original_quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());

  pass->quad_list.push_back(
      original_quad->Copy(pass->shared_quad_state_list.back()));
  // Add something behind it.
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);

  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());

  RenderPass* main_pass = pass_list.back();
  // Check that the quad is gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
  const QuadList& quad_list = main_pass->quad_list;
  for (QuadList::ConstBackToFrontIterator it = quad_list.BackToFrontBegin();
       it != quad_list.BackToFrontEnd();
       ++it) {
    EXPECT_NE(DrawQuad::TEXTURE_CONTENT, (*it)->material);
  }

  // Check that the right resource id got extracted.
  EXPECT_EQ(original_quad->resource_id, candidate_list.back().resource_id);
}

TEST_F(SingleOverlayOnTopTest, NoCandidates) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

TEST_F(SingleOverlayOnTopTest, OccludedCandidates) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  pass->quad_list.push_back(
      CreateCandidateQuad(resource_provider_.get(),
                          pass->shared_quad_state_list.back())
          .PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

// Test with multiple render passes.
TEST_F(SingleOverlayOnTopTest, MultipleRenderPasses) {
  RenderPassList pass_list;
  pass_list.push_back(CreateRenderPass());

  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> original_quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());

  pass->quad_list.push_back(
      original_quad->Copy(pass->shared_quad_state_list.back()));
  // Add something behind it.
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(2U, candidate_list.size());

  // This should be the same.
  ASSERT_EQ(2U, pass_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectPremultipliedAlpha) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  quad->premultiplied_alpha = true;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlending) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  quad->needs_blending = true;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBackgroundColor) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  quad->background_color = SK_ColorBLACK;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlendMode) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  pass->shared_quad_state_list.back()->blend_mode = SkXfermode::kScreen_Mode;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpacity) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  pass->shared_quad_state_list.back()->opacity = 0.5f;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  pass->shared_quad_state_list.back()->content_to_target_transform.Scale(2.f,
                                                                         2.f);

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

class OverlayInfoRendererGL : public GLRenderer {
 public:
  OverlayInfoRendererGL(RendererClient* client,
                        const LayerTreeSettings* settings,
                        OutputSurface* output_surface,
                        ResourceProvider* resource_provider)
      : GLRenderer(client,
                   settings,
                   output_surface,
                   resource_provider,
                   NULL,
                   0),
        expect_overlays_(false) {}

  MOCK_METHOD2(DoDrawQuad, void(DrawingFrame* frame, const DrawQuad* quad));

  virtual void FinishDrawingFrame(DrawingFrame* frame) OVERRIDE {
    GLRenderer::FinishDrawingFrame(frame);

    if (!expect_overlays_) {
      EXPECT_EQ(0U, frame->overlay_list.size());
      return;
    }

    ASSERT_EQ(2U, frame->overlay_list.size());
    EXPECT_NE(0U, frame->overlay_list.back().resource_id);
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
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual void RunOnDemandRasterTask(Task* on_demand_raster_task) OVERRIDE {}
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
        ResourceProvider::Create(output_surface_.get(), NULL, 0, false, 1,
        false);

    provider_->support()->SetScheduleOverlayPlaneCallback(base::Bind(
        &MockOverlayScheduler::Schedule, base::Unretained(&scheduler_)));
  }

  void Init(bool use_validator) {
    if (use_validator)
      output_surface_->InitWithSingleOverlayValidator();

    renderer_ =
        make_scoped_ptr(new OverlayInfoRendererGL(&renderer_client_,
                                                  &settings_,
                                                  output_surface_.get(),
                                                  resource_provider_.get()));
  }

  void SwapBuffers() { renderer_->SwapBuffers(CompositorFrameMetadata()); }

  LayerTreeSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<OverlayOutputSurface> output_surface_;
  FakeRendererClient renderer_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<OverlayInfoRendererGL> renderer_;
  scoped_refptr<TestContextProvider> provider_;
  MockOverlayScheduler scheduler_;
};

TEST_F(GLRendererWithOverlaysTest, OverlayQuadNotDrawn) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  gfx::Rect viewport_rect(16, 16);

  scoped_ptr<RenderPass> pass = CreateRenderPass();

  pass->quad_list.push_back(
      CreateCandidateQuad(resource_provider_.get(),
                          pass->shared_quad_state_list.back())
          .PassAs<DrawQuad>());

  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Candidate pass was taken out and extra skipped pass added,
  // so only draw 2 quads.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(2);
  EXPECT_CALL(scheduler_,
              Schedule(1,
                       gfx::OVERLAY_TRANSFORM_NONE,
                       _,
                       kOverlayRect,
                       BoundingRect(kUVTopLeft, kUVBottomRight))).Times(1);
  renderer_->DrawFrame(&pass_list, 1.f, viewport_rect, viewport_rect, false);

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, OccludedQuadDrawn) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(false);
  gfx::Rect viewport_rect(16, 16);

  scoped_ptr<RenderPass> pass = CreateRenderPass();

  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  pass->quad_list.push_back(
      CreateCandidateQuad(resource_provider_.get(),
                          pass->shared_quad_state_list.back())
          .PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // 3 quads in the pass, all should draw.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(3);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  renderer_->DrawFrame(&pass_list, 1.f, viewport_rect, viewport_rect, false);

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, NoValidatorNoOverlay) {
  bool use_validator = false;
  Init(use_validator);
  renderer_->set_expect_overlays(false);
  gfx::Rect viewport_rect(16, 16);

  scoped_ptr<RenderPass> pass = CreateRenderPass();

  pass->quad_list.push_back(
      CreateCandidateQuad(resource_provider_.get(),
                          pass->shared_quad_state_list.back())
          .PassAs<DrawQuad>());

  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Should see no overlays.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(3);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  renderer_->DrawFrame(&pass_list, 1.f, viewport_rect, viewport_rect, false);

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, ResourcesExportedAndReturned) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  ResourceProvider::ResourceId resource1 =
      CreateResource(resource_provider_.get());
  ResourceProvider::ResourceId resource2 =
      CreateResource(resource_provider_.get());

  DirectRenderer::DrawingFrame frame1;
  frame1.overlay_list.resize(2);
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.overlay_list.resize(2);
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = resource2;
  overlay2.plane_z_order = 1;

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->FinishDrawingFrame(&frame2);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // No overlays, release the resource.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  DirectRenderer::DrawingFrame frame3;
  renderer_->set_expect_overlays(false);
  renderer_->FinishDrawingFrame(&frame3);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // Use the same buffer twice.
  renderer_->set_expect_overlays(true);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  renderer_->set_expect_overlays(false);
  renderer_->FinishDrawingFrame(&frame3);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);
}

}  // namespace
}  // namespace cc
