// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_renderer_layer_impl.h"

#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/test/fake_delegated_renderer_layer_impl.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class DelegatedRendererLayerImplTest : public testing::Test {
 public:
  DelegatedRendererLayerImplTest()
      : proxy_(),
        always_impl_thread_and_main_thread_blocked_(&proxy_) {
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();

    host_impl_.reset(
        new FakeLayerTreeHostImpl(settings, &proxy_, &shared_bitmap_manager_));
    host_impl_->InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
    host_impl_->SetViewportSize(gfx::Size(10, 10));
  }

 protected:
  FakeProxy proxy_;
  DebugScopedSetImplThreadAndMainThreadBlocked
      always_impl_thread_and_main_thread_blocked_;
  TestSharedBitmapManager shared_bitmap_manager_;
  scoped_ptr<LayerTreeHostImpl> host_impl_;
};

class DelegatedRendererLayerImplTestSimple
    : public DelegatedRendererLayerImplTest {
 public:
  DelegatedRendererLayerImplTestSimple()
      : DelegatedRendererLayerImplTest() {
    scoped_ptr<LayerImpl> root_layer = SolidColorLayerImpl::Create(
        host_impl_->active_tree(), 1).PassAs<LayerImpl>();
    scoped_ptr<LayerImpl> layer_before = SolidColorLayerImpl::Create(
        host_impl_->active_tree(), 2).PassAs<LayerImpl>();
    scoped_ptr<LayerImpl> layer_after = SolidColorLayerImpl::Create(
        host_impl_->active_tree(), 3).PassAs<LayerImpl>();
    scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer =
        FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 4);

    host_impl_->SetViewportSize(gfx::Size(100, 100));
    root_layer->SetBounds(gfx::Size(100, 100));

    layer_before->SetPosition(gfx::Point(20, 20));
    layer_before->SetBounds(gfx::Size(14, 14));
    layer_before->SetContentBounds(gfx::Size(14, 14));
    layer_before->SetDrawsContent(true);
    layer_before->SetForceRenderSurface(true);

    layer_after->SetPosition(gfx::Point(5, 5));
    layer_after->SetBounds(gfx::Size(15, 15));
    layer_after->SetContentBounds(gfx::Size(15, 15));
    layer_after->SetDrawsContent(true);
    layer_after->SetForceRenderSurface(true);

    delegated_renderer_layer->SetPosition(gfx::Point(3, 3));
    delegated_renderer_layer->SetBounds(gfx::Size(10, 10));
    delegated_renderer_layer->SetContentBounds(gfx::Size(10, 10));
    delegated_renderer_layer->SetDrawsContent(true);
    gfx::Transform transform;
    transform.Translate(1.0, 1.0);
    delegated_renderer_layer->SetTransform(transform);

    ScopedPtrVector<RenderPass> delegated_render_passes;
    TestRenderPass* pass1 = AddRenderPass(&delegated_render_passes,
                                          RenderPass::Id(9, 6),
                                          gfx::Rect(6, 6, 6, 6),
                                          gfx::Transform(1, 0, 0, 1, 5, 6));
    AddQuad(pass1, gfx::Rect(0, 0, 6, 6), 33u);
    TestRenderPass* pass2 = AddRenderPass(&delegated_render_passes,
                                          RenderPass::Id(9, 7),
                                          gfx::Rect(7, 7, 7, 7),
                                          gfx::Transform(1, 0, 0, 1, 7, 8));
    AddQuad(pass2, gfx::Rect(0, 0, 7, 7), 22u);
    AddRenderPassQuad(pass2, pass1);
    TestRenderPass* pass3 = AddRenderPass(&delegated_render_passes,
                                          RenderPass::Id(9, 8),
                                          gfx::Rect(0, 0, 8, 8),
                                          gfx::Transform(1, 0, 0, 1, 9, 10));
    AddRenderPassQuad(pass3, pass2);
    delegated_renderer_layer->SetFrameDataForRenderPasses(
        1.f, &delegated_render_passes);

    // The RenderPasses should be taken by the layer.
    EXPECT_EQ(0u, delegated_render_passes.size());

    root_layer_ = root_layer.get();
    layer_before_ = layer_before.get();
    layer_after_ = layer_after.get();
    delegated_renderer_layer_ = delegated_renderer_layer.get();

    // Force the delegated RenderPasses to come before the RenderPass from
    // layer_after.
    layer_after->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
    root_layer->AddChild(layer_after.Pass());

    // Get the RenderPass generated by layer_before to come before the delegated
    // RenderPasses.
    root_layer->AddChild(layer_before.Pass());
    host_impl_->active_tree()->SetRootLayer(root_layer.Pass());
  }

 protected:
  LayerImpl* root_layer_;
  LayerImpl* layer_before_;
  LayerImpl* layer_after_;
  DelegatedRendererLayerImpl* delegated_renderer_layer_;
};

TEST_F(DelegatedRendererLayerImplTestSimple, AddsContributingRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);
  // And all other RenderPasses should be non-delegated.
  EXPECT_NE(4, frame.render_passes[0]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[0]->id.index);
  EXPECT_NE(4, frame.render_passes[3]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[3]->id.index);
  EXPECT_NE(4, frame.render_passes[4]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[4]->id.index);

  // The DelegatedRendererLayer should have added its RenderPasses to the frame
  // in order.
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[1]->output_rect.ToString());
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[2]->output_rect.ToString());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple,
       AddsQuadsToContributingRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);

  // The DelegatedRendererLayer should have added copies of its quads to
  // contributing RenderPasses.
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  // Verify it added the right quads.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 7).ToString(),
            frame.render_passes[2]->quad_list[0]->rect.ToString());
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[2]->quad_list[1]->rect.ToString());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple, AddsQuadsToTargetRenderPass) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The layer's target is the RenderPass from layer_after_.
  EXPECT_EQ(RenderPass::Id(3, 0), frame.render_passes[3]->id);

  // The DelegatedRendererLayer should have added copies of quads in its root
  // RenderPass to its target RenderPass. The layer_after_ also adds one quad.
  ASSERT_EQ(2u, frame.render_passes[3]->quad_list.size());

  // Verify it added the right quads.
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[3]->quad_list[0]->rect.ToString());

  // Its target layer should have a quad as well.
  EXPECT_EQ(gfx::Rect(0, 0, 15, 15).ToString(),
            frame.render_passes[3]->quad_list[1]->rect.ToString());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple,
       QuadsFromRootRenderPassAreModifiedForTheTarget) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The DelegatedRendererLayer is at position 3,3 compared to its target, and
  // has a translation transform of 1,1. So its root RenderPass' quads should
  // all be transformed by that combined amount.
  gfx::Transform transform;
  transform.Translate(4.0, 4.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      transform, frame.render_passes[3]->quad_list[0]->quadTransform());

  // Quads from non-root RenderPasses should not be shifted though.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[0]->quadTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[1]->quadTransform());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[1]->quad_list[0]->quadTransform());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple, RenderPassTransformIsModified) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // The delegated layer has a surface between it and the root.
  EXPECT_TRUE(delegated_renderer_layer_->render_target()->parent());

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The DelegatedRendererLayer is at position 9,9 compared to the root, so all
  // render pass' transforms to the root should be shifted by this amount.
  gfx::Transform transform;
  transform.Translate(9.0, 9.0);

  // The first contributing surface has a translation of 5, 6.
  gfx::Transform five_six(1, 0, 0, 1, 5, 6);

  // The second contributing surface has a translation of 7, 8.
  gfx::Transform seven_eight(1, 0, 0, 1, 7, 8);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      transform * five_six, frame.render_passes[1]->transform_to_root_target);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      transform * seven_eight,
      frame.render_passes[2]->transform_to_root_target);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple, DoesNotOwnARenderSurface) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // If the DelegatedRendererLayer is axis aligned and has opacity 1, then it
  // has no need to be a RenderSurface for the quads it carries.
  EXPECT_FALSE(delegated_renderer_layer_->render_surface());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple, DoesOwnARenderSurfaceForOpacity) {
  delegated_renderer_layer_->SetOpacity(0.5f);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // This test case has quads from multiple layers in the delegated renderer, so
  // if the DelegatedRendererLayer has opacity < 1, it should end up with a
  // render surface.
  EXPECT_TRUE(delegated_renderer_layer_->render_surface());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple,
       DoesOwnARenderSurfaceForTransform) {
  gfx::Transform rotation;
  rotation.RotateAboutZAxis(30.0);
  delegated_renderer_layer_->SetTransform(rotation);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // This test case has quads from multiple layers in the delegated renderer, so
  // if the DelegatedRendererLayer has opacity < 1, it should end up with a
  // render surface.
  EXPECT_TRUE(delegated_renderer_layer_->render_surface());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

class DelegatedRendererLayerImplTestOwnSurface
    : public DelegatedRendererLayerImplTestSimple {
 public:
  DelegatedRendererLayerImplTestOwnSurface()
      : DelegatedRendererLayerImplTestSimple() {
    delegated_renderer_layer_->SetForceRenderSurface(true);
  }
};

TEST_F(DelegatedRendererLayerImplTestOwnSurface, AddsRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);
  // The DelegatedRendererLayer should have added a RenderPass for its surface
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[3]->id.index);
  // And all other RenderPasses should be non-delegated.
  EXPECT_NE(4, frame.render_passes[0]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[0]->id.index);
  EXPECT_NE(4, frame.render_passes[4]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[4]->id.index);
  EXPECT_NE(4, frame.render_passes[5]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[5]->id.index);

  // The DelegatedRendererLayer should have added its RenderPasses to the frame
  // in order.
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[1]->output_rect.ToString());
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[2]->output_rect.ToString());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestOwnSurface,
       AddsQuadsToContributingRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);

  // The DelegatedRendererLayer should have added copies of its quads to
  // contributing RenderPasses.
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  // Verify it added the right quads.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 7).ToString(),
            frame.render_passes[2]->quad_list[0]->rect.ToString());
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[2]->quad_list[1]->rect.ToString());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestOwnSurface, AddsQuadsToTargetRenderPass) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // The layer's target is the RenderPass owned by itself.
  EXPECT_EQ(RenderPass::Id(4, 0), frame.render_passes[3]->id);

  // The DelegatedRendererLayer should have added copies of quads in its root
  // RenderPass to its target RenderPass.
  // The layer_after also adds one quad.
  ASSERT_EQ(1u, frame.render_passes[3]->quad_list.size());

  // Verify it added the right quads.
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[3]->quad_list[0]->rect.ToString());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestOwnSurface,
       QuadsFromRootRenderPassAreNotModifiedForTheTarget) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // Because the DelegatedRendererLayer owns a RenderSurfaceImpl, its root
  // RenderPass' quads do not need to be translated at all.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[3]->quad_list[0]->quadTransform());

  // Quads from non-root RenderPasses should not be shifted either.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[0]->quadTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[1]->quadTransform());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[1]->quad_list[0]->quadTransform());

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

class DelegatedRendererLayerImplTestTransform
    : public DelegatedRendererLayerImplTest {
 public:
  DelegatedRendererLayerImplTestTransform()
      : root_delegated_render_pass_is_clipped_(false),
        delegated_device_scale_factor_(2.f) {}

  void SetUpTest() {
    host_impl_->SetDeviceScaleFactor(2.f);

    scoped_ptr<LayerImpl> root_layer = LayerImpl::Create(
        host_impl_->active_tree(), 1);
    scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer =
        FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 2);

    host_impl_->SetViewportSize(gfx::Size(200, 200));
    root_layer->SetBounds(gfx::Size(100, 100));

    delegated_renderer_layer->SetPosition(gfx::Point(20, 20));
    delegated_renderer_layer->SetBounds(gfx::Size(75, 75));
    delegated_renderer_layer->SetContentBounds(gfx::Size(75, 75));
    delegated_renderer_layer->SetDrawsContent(true);
    gfx::Transform transform;
    transform.Scale(2.0, 2.0);
    transform.Translate(8.0, 8.0);
    delegated_renderer_layer->SetTransform(transform);

    ScopedPtrVector<RenderPass> delegated_render_passes;

    gfx::Size child_pass_content_bounds(7, 7);
    gfx::Rect child_pass_rect(20, 20, 7, 7);
    gfx::Transform child_pass_transform;
    child_pass_transform.Scale(0.8f, 0.8f);
    child_pass_transform.Translate(9.0, 9.0);
    gfx::Rect child_pass_clip_rect(21, 21, 3, 3);
    bool child_pass_clipped = false;

    {
      TestRenderPass* pass = AddRenderPass(
          &delegated_render_passes,
          RenderPass::Id(10, 7),
          child_pass_rect,
          gfx::Transform());
      SharedQuadState* shared_quad_state =
          pass->CreateAndAppendSharedQuadState();
      shared_quad_state->SetAll(child_pass_transform,
                                child_pass_content_bounds,
                                child_pass_rect,
                                child_pass_clip_rect,
                                child_pass_clipped,
                                1.f,
                                SkXfermode::kSrcOver_Mode,
                                0);

      scoped_ptr<SolidColorDrawQuad> color_quad;
      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state,
                         gfx::Rect(20, 20, 3, 7),
                         gfx::Rect(20, 20, 3, 7),
                         1u,
                         false);
      pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state,
                         gfx::Rect(23, 20, 4, 7),
                         gfx::Rect(23, 20, 4, 7),
                         1u,
                         false);
      pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());
    }

    gfx::Size root_pass_content_bounds(100, 100);
    gfx::Rect root_pass_rect(0, 0, 100, 100);
    gfx::Transform root_pass_transform;
    root_pass_transform.Scale(1.5, 1.5);
    root_pass_transform.Translate(7.0, 7.0);
    gfx::Rect root_pass_clip_rect(10, 10, 35, 35);
    bool root_pass_clipped = root_delegated_render_pass_is_clipped_;

    TestRenderPass* pass = AddRenderPass(
        &delegated_render_passes,
        RenderPass::Id(9, 6),
        root_pass_rect,
        gfx::Transform());
    SharedQuadState* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(root_pass_transform,
                              root_pass_content_bounds,
                              root_pass_rect,
                              root_pass_clip_rect,
                              root_pass_clipped,
                              1.f,
                              SkXfermode::kSrcOver_Mode,
                              0);

    scoped_ptr<RenderPassDrawQuad> render_pass_quad =
        RenderPassDrawQuad::Create();
    render_pass_quad->SetNew(
        shared_quad_state,
        gfx::Rect(5, 5, 7, 7),  // quad_rect
        gfx::Rect(5, 5, 7, 7),  // visible_rect
        RenderPass::Id(10, 7),  // render_pass_id
        false,                  // is_replica
        0,                      // mask_resource_id
        child_pass_rect,        // contents_changed_since_last_frame
        gfx::RectF(),           // mask_uv_rect
        FilterOperations(),     // filters
        FilterOperations());    // background_filters
    pass->AppendDrawQuad(render_pass_quad.PassAs<DrawQuad>());

    scoped_ptr<SolidColorDrawQuad> color_quad;
    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(0, 0, 10, 10),
                       gfx::Rect(0, 0, 10, 10),
                       1u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(0, 10, 10, 10),
                       gfx::Rect(0, 10, 10, 10),
                       2u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(10, 0, 10, 10),
                       gfx::Rect(10, 0, 10, 10),
                       3u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(10, 10, 10, 10),
                       gfx::Rect(10, 10, 10, 10),
                       4u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    delegated_renderer_layer->SetFrameDataForRenderPasses(
        delegated_device_scale_factor_, &delegated_render_passes);

    // The RenderPasses should be taken by the layer.
    EXPECT_EQ(0u, delegated_render_passes.size());

    root_layer_ = root_layer.get();
    delegated_renderer_layer_ = delegated_renderer_layer.get();

    root_layer->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
    host_impl_->active_tree()->SetRootLayer(root_layer.Pass());
  }

  void VerifyRenderPasses(
      const LayerTreeHostImpl::FrameData& frame,
      size_t num_render_passes,
      const SharedQuadState** root_delegated_shared_quad_state,
      const SharedQuadState** contrib_delegated_shared_quad_state) {
    ASSERT_EQ(num_render_passes, frame.render_passes.size());
    // The contributing render pass in the DelegatedRendererLayer.
    EXPECT_EQ(2, frame.render_passes[0]->id.layer_id);
    EXPECT_EQ(1, frame.render_passes[0]->id.index);
    // The root render pass.
    EXPECT_EQ(1, frame.render_passes.back()->id.layer_id);
    EXPECT_EQ(0, frame.render_passes.back()->id.index);

    const QuadList& contrib_delegated_quad_list =
        frame.render_passes[0]->quad_list;
    ASSERT_EQ(2u, contrib_delegated_quad_list.size());

    const QuadList& root_delegated_quad_list =
        frame.render_passes[1]->quad_list;
    ASSERT_EQ(5u, root_delegated_quad_list.size());

    // All quads in a render pass should share the same state.
    *contrib_delegated_shared_quad_state =
        contrib_delegated_quad_list[0]->shared_quad_state;
    EXPECT_EQ(*contrib_delegated_shared_quad_state,
              contrib_delegated_quad_list[1]->shared_quad_state);

    *root_delegated_shared_quad_state =
        root_delegated_quad_list[0]->shared_quad_state;
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[1]->shared_quad_state);
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[2]->shared_quad_state);
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[3]->shared_quad_state);
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[4]->shared_quad_state);

    EXPECT_NE(*contrib_delegated_shared_quad_state,
              *root_delegated_shared_quad_state);
  }

 protected:
  LayerImpl* root_layer_;
  DelegatedRendererLayerImpl* delegated_renderer_layer_;
  bool root_delegated_render_pass_is_clipped_;
  float delegated_device_scale_factor_;
};

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsUnclipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = false;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      2,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // When the quads don't have a clip of their own, the clip rect is set to
  // the drawable_content_rect of the delegated renderer layer.
  EXPECT_EQ(delegated_renderer_layer_->drawable_content_rect().ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // Even though the quads in the root pass have no clip of their own, they
  // inherit the clip rect from the delegated renderer layer if it does not
  // own a surface.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  // Device scale factor.
  expected.Scale(2.0, 2.0);
  // This is the transform from the layer's space to its target.
  expected.Translate(20, 20);
  expected.Scale(2.0, 2.0);
  expected.Translate(8.0, 8.0);
  // This is the transform within the source frame.
  // Inverse device scale factor to go from physical space to layer space.
  expected.Scale(0.5, 0.5);
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8f, 0.8f);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsClipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = true;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      2,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // Since the quads have a clip_rect it should be modified by delegated
  // renderer layer's draw_transform.
  // The position of the resulting clip_rect is:
  // (clip rect position (10) * inverse dsf (1/2) + translate (8)) *
  //     layer scale (2) + layer position (20) = 46
  // The device scale is 2, so everything gets doubled, giving 92.
  //
  // The size is 35x35 scaled by the device scale.
  EXPECT_EQ(gfx::Rect(92, 92, 70, 70).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // The quads had a clip and it should be preserved.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  // Device scale factor.
  expected.Scale(2.0, 2.0);
  // This is the transform from the layer's space to its target.
  expected.Translate(20, 20);
  expected.Scale(2.0, 2.0);
  expected.Translate(8.0, 8.0);
  // This is the transform within the source frame.
  // Inverse device scale factor to go from physical space to layer space.
  expected.Scale(0.5, 0.5);
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8f, 0.8f);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsUnclipped_Surface) {
  root_delegated_render_pass_is_clipped_ = false;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      3,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // When the layer owns a surface, then its position and translation are not
  // a part of its draw transform.
  EXPECT_EQ(gfx::Rect(10, 10, 35, 35).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // Since the layer owns a surface it doesn't need to clip its quads, so
  // unclipped quads remain unclipped.
  EXPECT_FALSE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  // This is the transform within the source frame.
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8f, 0.8f);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsClipped_Surface) {
  root_delegated_render_pass_is_clipped_ = true;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      3,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // When the layer owns a surface, then its position and translation are not
  // a part of its draw transform. The clip_rect should be preserved.
  EXPECT_EQ(gfx::Rect(10, 10, 35, 35).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // The quads had a clip and it should be preserved.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  // This is the transform within the source frame.
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8f, 0.8f);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestTransform, MismatchedDeviceScaleFactor) {
  root_delegated_render_pass_is_clipped_ = true;
  delegated_device_scale_factor_ = 1.3f;

  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(frame,
                     2,
                     &root_delegated_shared_quad_state,
                     &contrib_delegated_shared_quad_state);

  // The parent tree's device scale factor is 2.0, but the child has submitted a
  // frame with a device scale factor of 1.3.  Absent any better option, the
  // only thing we can do is scale from 1.3 -> 2.0.

  gfx::Transform expected;
  // Device scale factor (from parent).
  expected.Scale(2.0, 2.0);
  // This is the transform from the layer's space to its target.
  expected.Translate(20, 20);
  expected.Scale(2.0, 2.0);
  expected.Translate(8.0, 8.0);
  // This is the transform within the source frame.
  // Inverse device scale factor (from child).
  expected.Scale(1.0f / 1.3f, 1.0f / 1.3f);
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

class DelegatedRendererLayerImplTestClip
    : public DelegatedRendererLayerImplTest {
 public:
  void SetUpTest() {
    scoped_ptr<LayerImpl> root_layer =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer =
        FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 2);
    scoped_ptr<LayerImpl> clip_layer =
        LayerImpl::Create(host_impl_->active_tree(), 3);
    scoped_ptr<LayerImpl> origin_layer =
        LayerImpl::Create(host_impl_->active_tree(), 4);

    host_impl_->SetViewportSize(gfx::Size(100, 100));
    root_layer->SetBounds(gfx::Size(100, 100));

    delegated_renderer_layer->SetPosition(gfx::Point(20, 20));
    delegated_renderer_layer->SetBounds(gfx::Size(50, 50));
    delegated_renderer_layer->SetContentBounds(gfx::Size(50, 50));
    delegated_renderer_layer->SetDrawsContent(true);

    ScopedPtrVector<RenderPass> delegated_render_passes;

    gfx::Size child_pass_content_bounds(7, 7);
    gfx::Rect child_pass_rect(20, 20, 7, 7);
    gfx::Transform child_pass_transform;
    gfx::Rect child_pass_clip_rect(21, 21, 3, 3);
    bool child_pass_clipped = false;

    {
      TestRenderPass* pass = AddRenderPass(
          &delegated_render_passes,
          RenderPass::Id(10, 7),
          child_pass_rect,
          gfx::Transform());
      SharedQuadState* shared_quad_state =
          pass->CreateAndAppendSharedQuadState();
      shared_quad_state->SetAll(child_pass_transform,
                                child_pass_content_bounds,
                                child_pass_rect,
                                child_pass_clip_rect,
                                child_pass_clipped,
                                1.f,
                                SkXfermode::kSrcOver_Mode,
                                0);

      scoped_ptr<SolidColorDrawQuad> color_quad;
      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state,
                         gfx::Rect(20, 20, 3, 7),
                         gfx::Rect(20, 20, 3, 7),
                         1u,
                         false);
      pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state,
                         gfx::Rect(23, 20, 4, 7),
                         gfx::Rect(23, 20, 4, 7),
                         1u,
                         false);
      pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());
    }

    gfx::Size root_pass_content_bounds(50, 50);
    gfx::Rect root_pass_rect(0, 0, 50, 50);
    gfx::Transform root_pass_transform;
    gfx::Rect root_pass_clip_rect(5, 5, 40, 40);
    bool root_pass_clipped = root_delegated_render_pass_is_clipped_;

    TestRenderPass* pass = AddRenderPass(
        &delegated_render_passes,
        RenderPass::Id(9, 6),
        root_pass_rect,
        gfx::Transform());
    SharedQuadState* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(root_pass_transform,
                              root_pass_content_bounds,
                              root_pass_rect,
                              root_pass_clip_rect,
                              root_pass_clipped,
                              1.f,
                              SkXfermode::kSrcOver_Mode,
                              0);

    scoped_ptr<RenderPassDrawQuad> render_pass_quad =
        RenderPassDrawQuad::Create();
    render_pass_quad->SetNew(
        shared_quad_state,
        gfx::Rect(5, 5, 7, 7),  // quad_rect
        gfx::Rect(5, 5, 7, 7),  // visible_quad_rect
        RenderPass::Id(10, 7),  // render_pass_id
        false,                  // is_replica
        0,                      // mask_resource_id
        child_pass_rect,        // contents_changed_since_last_frame
        gfx::RectF(),           // mask_uv_rect
        FilterOperations(),     // filters
        FilterOperations());    // background_filters
    pass->AppendDrawQuad(render_pass_quad.PassAs<DrawQuad>());

    scoped_ptr<SolidColorDrawQuad> color_quad;
    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(0, 0, 10, 10),
                       gfx::Rect(0, 0, 10, 10),
                       1u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(0, 10, 10, 10),
                       gfx::Rect(0, 10, 10, 10),
                       2u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(10, 0, 10, 10),
                       gfx::Rect(10, 0, 10, 10),
                       3u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state,
                       gfx::Rect(10, 10, 10, 10),
                       gfx::Rect(10, 10, 10, 10),
                       4u,
                       false);
    pass->AppendDrawQuad(color_quad.PassAs<DrawQuad>());

    delegated_renderer_layer->SetFrameDataForRenderPasses(
        1.f, &delegated_render_passes);

    // The RenderPasses should be taken by the layer.
    EXPECT_EQ(0u, delegated_render_passes.size());

    root_layer_ = root_layer.get();
    delegated_renderer_layer_ = delegated_renderer_layer.get();

    if (clip_delegated_renderer_layer_) {
      gfx::Rect clip_rect(21, 27, 23, 21);

      clip_layer->SetPosition(clip_rect.origin());
      clip_layer->SetBounds(clip_rect.size());
      clip_layer->SetContentBounds(clip_rect.size());
      clip_layer->SetMasksToBounds(true);

      origin_layer->SetPosition(
          gfx::PointAtOffsetFromOrigin(-clip_rect.OffsetFromOrigin()));

      origin_layer->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
      clip_layer->AddChild(origin_layer.Pass());
      root_layer->AddChild(clip_layer.Pass());
    } else {
      root_layer->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
    }

    host_impl_->active_tree()->SetRootLayer(root_layer.Pass());
  }

 protected:
  LayerImpl* root_layer_;
  DelegatedRendererLayerImpl* delegated_renderer_layer_;
  bool root_delegated_render_pass_is_clipped_;
  bool clip_delegated_renderer_layer_;
};

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerUnclipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the quads don't have a clip of their own, the clip rect is set to
  // the drawable_content_rect of the delegated renderer layer.
  EXPECT_EQ(gfx::Rect(20, 20, 50, 50).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads are clipped to the delegated renderer layer.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsClipped_LayerUnclipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list =
      frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used.
  EXPECT_EQ(gfx::Rect(25, 25, 40, 40).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerClipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the quads don't have a clip of their own, the clip rect is set to
  // the drawable_content_rect of the delegated renderer layer. When the layer
  // is clipped, that should be seen in the quads' clip_rect.
  EXPECT_EQ(gfx::Rect(21, 27, 23, 21).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads are clipped to the delegated renderer layer.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsClipped_LayerClipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used, but it is
  // combined with the clip rect of the delegated renderer layer.
  EXPECT_EQ(gfx::Rect(25, 27, 19, 21).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerUnclipped_Surface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the layer owns a surface, the quads don't need to be clipped
  // further than they already specify. If they aren't clipped, then their
  // clip rect is ignored, and they are not set as clipped.
  EXPECT_FALSE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsClipped_LayerUnclipped_Surface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used.
  EXPECT_EQ(gfx::Rect(5, 5, 40, 40).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerClipped_Surface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the layer owns a surface, the quads don't need to be clipped
  // further than they already specify. If they aren't clipped, then their
  // clip rect is ignored, and they are not set as clipped.
  EXPECT_FALSE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip, QuadsClipped_LayerClipped_Surface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used, but it is
  // combined with the clip rect of the delegated renderer layer. If the
  // layer owns a surface, then it does not have a clip rect of its own.
  EXPECT_EQ(gfx::Rect(5, 5, 40, 40).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTest, InvalidRenderPassDrawQuad) {
  scoped_ptr<LayerImpl> root_layer = LayerImpl::Create(
      host_impl_->active_tree(), 1).PassAs<LayerImpl>();
  scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer =
      FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 4);

  host_impl_->SetViewportSize(gfx::Size(100, 100));

  delegated_renderer_layer->SetPosition(gfx::Point(3, 3));
  delegated_renderer_layer->SetBounds(gfx::Size(10, 10));
  delegated_renderer_layer->SetContentBounds(gfx::Size(10, 10));
  delegated_renderer_layer->SetDrawsContent(true);

  ScopedPtrVector<RenderPass> delegated_render_passes;
  TestRenderPass* pass1 = AddRenderPass(
      &delegated_render_passes,
      RenderPass::Id(9, 6),
      gfx::Rect(0, 0, 10, 10),
      gfx::Transform());
  AddQuad(pass1, gfx::Rect(0, 0, 6, 6), 33u);

  // This render pass isn't part of the frame.
  scoped_ptr<TestRenderPass> missing_pass(TestRenderPass::Create());
  missing_pass->SetNew(RenderPass::Id(9, 7),
                       gfx::Rect(7, 7, 7, 7),
                       gfx::Rect(7, 7, 7, 7),
                       gfx::Transform());

  // But a render pass quad refers to it.
  AddRenderPassQuad(pass1, missing_pass.get());

  delegated_renderer_layer->SetFrameDataForRenderPasses(
      1.f, &delegated_render_passes);

  // The RenderPasses should be taken by the layer.
  EXPECT_EQ(0u, delegated_render_passes.size());

  root_layer->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
  host_impl_->active_tree()->SetRootLayer(root_layer.Pass());

  LayerTreeHostImpl::FrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  // The DelegatedRendererLayerImpl should drop the bad RenderPassDrawQuad.
  ASSERT_EQ(1u, frame.render_passes.size());
  ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
  EXPECT_EQ(DrawQuad::SOLID_COLOR,
            frame.render_passes[0]->quad_list[0]->material);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);
  gfx::Rect quad_rect(200, 300, 400, 500);

  gfx::Transform transform;
  transform.Translate(11.0, 0.0);

  LayerTestCommon::LayerImplTest impl;

  FakeDelegatedRendererLayerImpl* delegated_renderer_layer_impl =
      impl.AddChildToRoot<FakeDelegatedRendererLayerImpl>();
  delegated_renderer_layer_impl->SetBounds(layer_size);
  delegated_renderer_layer_impl->SetContentBounds(layer_size);
  delegated_renderer_layer_impl->SetDrawsContent(true);

  ScopedPtrVector<RenderPass> delegated_render_passes;
  // pass2 is just the size of the quad. It contributes to |pass1| with a
  // translation of (11,0).
  RenderPass::Id pass2_id =
      delegated_renderer_layer_impl->FirstContributingRenderPassId();
  TestRenderPass* pass2 =
      AddRenderPass(&delegated_render_passes, pass2_id, quad_rect, transform);
  AddQuad(pass2, gfx::Rect(quad_rect.size()), SK_ColorRED);
  // |pass1| covers the whole layer.
  RenderPass::Id pass1_id = RenderPass::Id(impl.root_layer()->id(), 0);
  TestRenderPass* pass1 = AddRenderPass(&delegated_render_passes,
                                        pass1_id,
                                        gfx::Rect(layer_size),
                                        gfx::Transform());
  AddRenderPassQuad(pass1, pass2, 0, FilterOperations(), transform);
  delegated_renderer_layer_impl->SetFrameDataForRenderPasses(
      1.f, &delegated_render_passes);

  impl.CalcDrawProps(viewport_size);

  // The |quad_rect| translated by the |transform|.
  gfx::Rect quad_screen_rect = quad_rect + gfx::Vector2d(11, 0);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;

    {
      SCOPED_TRACE("Root render pass");
      impl.AppendQuadsForPassWithOcclusion(
          delegated_renderer_layer_impl, pass1_id, occluded);
      LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                   quad_screen_rect);
      ASSERT_EQ(1u, impl.quad_list().size());
      EXPECT_EQ(DrawQuad::RENDER_PASS, impl.quad_list()[0]->material);
    }
    {
      SCOPED_TRACE("Contributing render pass");
      impl.AppendQuadsForPassWithOcclusion(
          delegated_renderer_layer_impl, pass2_id, occluded);
      LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                   gfx::Rect(quad_rect.size()));
      ASSERT_EQ(1u, impl.quad_list().size());
      EXPECT_EQ(DrawQuad::SOLID_COLOR, impl.quad_list()[0]->material);
    }
  }

  {
    SCOPED_TRACE("Full occlusion");
    {
      gfx::Rect occluded(delegated_renderer_layer_impl->visible_content_rect());

      SCOPED_TRACE("Root render pass");
      impl.AppendQuadsForPassWithOcclusion(
          delegated_renderer_layer_impl, pass1_id, occluded);
      LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                   gfx::Rect());
      EXPECT_EQ(impl.quad_list().size(), 0u);
    }
    {
      gfx::Rect occluded(delegated_renderer_layer_impl->visible_content_rect());
      // Move the occlusion to where it is in the contributing surface.
      occluded -= quad_rect.OffsetFromOrigin();

      SCOPED_TRACE("Contributing render pass");
      impl.AppendQuadsForPassWithOcclusion(
          delegated_renderer_layer_impl, pass2_id, occluded);
      LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                   gfx::Rect());
      EXPECT_EQ(impl.quad_list().size(), 0u);
    }
  }

  {
    SCOPED_TRACE("Partial occlusion");
    {
      gfx::Rect occluded(0, 0, 500, 1000);

      SCOPED_TRACE("Root render pass");
      impl.AppendQuadsForPassWithOcclusion(
          delegated_renderer_layer_impl, pass1_id, occluded);
      size_t partially_occluded_count = 0;
      LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
          impl.quad_list(),
          quad_screen_rect,
          occluded,
          &partially_occluded_count);
      // The layer outputs one quad, which is partially occluded.
      EXPECT_EQ(1u, impl.quad_list().size());
      EXPECT_EQ(1u, partially_occluded_count);
    }
    {
      gfx::Rect occluded(0, 0, 500, 1000);
      // Move the occlusion to where it is in the contributing surface.
      occluded -= quad_rect.OffsetFromOrigin() + gfx::Vector2d(11, 0);

      SCOPED_TRACE("Contributing render pass");
      impl.AppendQuadsForPassWithOcclusion(
          delegated_renderer_layer_impl, pass2_id, occluded);
      size_t partially_occluded_count = 0;
      LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
          impl.quad_list(),
          gfx::Rect(quad_rect.size()),
          occluded,
          &partially_occluded_count);
      // The layer outputs one quad, which is partially occluded.
      EXPECT_EQ(1u, impl.quad_list().size());
      EXPECT_EQ(1u, partially_occluded_count);
      // The quad in the contributing surface is at (211,300) in the root.
      // The occlusion extends to 500 in the x-axis, pushing the left of the
      // visible part of the quad to 500 - 211 = 300 - 11 inside the quad.
      EXPECT_EQ(gfx::Rect(300 - 11, 0, 100 + 11, 500).ToString(),
                impl.quad_list()[0]->visible_rect.ToString());
    }
  }
  {
    gfx::Rect occluded(0, 0, 500, 1000);
    // Move the occlusion to where it is in the contributing surface.
    occluded -= quad_rect.OffsetFromOrigin() + gfx::Vector2d(11, 0);

    SCOPED_TRACE("Contributing render pass with transformed root");

    delegated_renderer_layer_impl->SetTransform(transform);
    impl.CalcDrawProps(viewport_size);

    impl.AppendQuadsForPassWithOcclusion(
        delegated_renderer_layer_impl, pass2_id, occluded);
    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
        impl.quad_list(),
        gfx::Rect(quad_rect.size()),
        occluded,
        &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
    // The quad in the contributing surface is at (222,300) in the transformed
    // root. The occlusion extends to 500 in the x-axis, pushing the left of the
    // visible part of the quad to 500 - 222 = 300 - 22 inside the quad.
    EXPECT_EQ(gfx::Rect(300 - 22, 0, 100 + 22, 500).ToString(),
              impl.quad_list()[0]->visible_rect.ToString());
  }
}

TEST_F(DelegatedRendererLayerImplTest, PushPropertiesTo) {
  gfx::Size layer_size(1000, 1000);

  scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer_impl =
      FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 5);
  delegated_renderer_layer_impl->SetBounds(layer_size);
  delegated_renderer_layer_impl->SetContentBounds(layer_size);
  delegated_renderer_layer_impl->SetDrawsContent(true);

  RenderPassList delegated_render_passes;
  // |pass1| covers the whole layer.
  RenderPass::Id pass1_id = RenderPass::Id(5, 0);
  AddRenderPass(&delegated_render_passes,
                pass1_id,
                gfx::Rect(layer_size),
                gfx::Transform());
  delegated_renderer_layer_impl->SetFrameDataForRenderPasses(
      2.f, &delegated_render_passes);
  EXPECT_EQ(0.5f, delegated_renderer_layer_impl->inverse_device_scale_factor());

  scoped_ptr<DelegatedRendererLayerImpl> other_layer =
      DelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 6);

  delegated_renderer_layer_impl->PushPropertiesTo(other_layer.get());

  EXPECT_EQ(0.5f, other_layer->inverse_device_scale_factor());
}

}  // namespace
}  // namespace cc
