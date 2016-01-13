// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SURFACE_CHANGED(code_to_test)                       \
  render_surface->ResetPropertyChangedFlag();                                  \
  code_to_test;                                                                \
  EXPECT_TRUE(render_surface->SurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(code_to_test)                \
  render_surface->ResetPropertyChangedFlag();                                  \
  code_to_test;                                                                \
  EXPECT_FALSE(render_surface->SurfacePropertyChanged())

TEST(RenderSurfaceTest, VerifySurfaceChangesAreTrackedProperly) {
  //
  // This test checks that SurfacePropertyChanged() has the correct behavior.
  //

  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  scoped_ptr<LayerImpl> owning_layer =
      LayerImpl::Create(host_impl.active_tree(), 1);
  owning_layer->CreateRenderSurface();
  ASSERT_TRUE(owning_layer->render_surface());
  RenderSurfaceImpl* render_surface = owning_layer->render_surface();
  gfx::Rect test_rect(3, 4, 5, 6);
  owning_layer->ResetAllChangeTrackingForSubtree();

  // Currently, the content_rect, clip_rect, and
  // owning_layer->layerPropertyChanged() are the only sources of change.
  EXECUTE_AND_VERIFY_SURFACE_CHANGED(render_surface->SetClipRect(test_rect));
  EXECUTE_AND_VERIFY_SURFACE_CHANGED(render_surface->SetContentRect(test_rect));

  owning_layer->SetOpacity(0.5f);
  EXPECT_TRUE(render_surface->SurfacePropertyChanged());
  owning_layer->ResetAllChangeTrackingForSubtree();

  // Setting the surface properties to the same values again should not be
  // considered "change".
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetClipRect(test_rect));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetContentRect(test_rect));

  scoped_ptr<LayerImpl> dummy_mask =
      LayerImpl::Create(host_impl.active_tree(), 2);
  gfx::Transform dummy_matrix;
  dummy_matrix.Translate(1.0, 2.0);

  // The rest of the surface properties are either internal and should not cause
  // change, or they are already accounted for by the
  // owninglayer->layerPropertyChanged().
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetDrawOpacity(0.5f));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetDrawTransform(dummy_matrix));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetReplicaDrawTransform(dummy_matrix));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(render_surface->ClearLayerLists());
}

TEST(RenderSurfaceTest, SanityCheckSurfaceCreatesCorrectSharedQuadState) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  scoped_ptr<LayerImpl> root_layer =
      LayerImpl::Create(host_impl.active_tree(), 1);

  scoped_ptr<LayerImpl> owning_layer =
      LayerImpl::Create(host_impl.active_tree(), 2);
  owning_layer->CreateRenderSurface();
  ASSERT_TRUE(owning_layer->render_surface());
  owning_layer->draw_properties().render_target = owning_layer.get();
  RenderSurfaceImpl* render_surface = owning_layer->render_surface();

  root_layer->AddChild(owning_layer.Pass());

  gfx::Rect content_rect(0, 0, 50, 50);
  gfx::Rect clip_rect(5, 5, 40, 40);
  gfx::Transform origin;

  origin.Translate(30, 40);

  render_surface->SetDrawTransform(origin);
  render_surface->SetContentRect(content_rect);
  render_surface->SetClipRect(clip_rect);
  render_surface->SetDrawOpacity(1.f);

  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller mock_quad_culler(render_pass.get(), &occlusion_tracker);
  AppendQuadsData append_quads_data;

  bool for_replica = false;
  render_surface->AppendQuads(
      &mock_quad_culler, &append_quads_data, for_replica, RenderPass::Id(2, 0));

  ASSERT_EQ(1u, render_pass->shared_quad_state_list.size());
  SharedQuadState* shared_quad_state = render_pass->shared_quad_state_list[0];

  EXPECT_EQ(
      30.0,
      shared_quad_state->content_to_target_transform.matrix().getDouble(0, 3));
  EXPECT_EQ(
      40.0,
      shared_quad_state->content_to_target_transform.matrix().getDouble(1, 3));
  EXPECT_RECT_EQ(content_rect,
                 gfx::Rect(shared_quad_state->visible_content_rect));
  EXPECT_EQ(1.f, shared_quad_state->opacity);
}

class TestRenderPassSink : public RenderPassSink {
 public:
  virtual void AppendRenderPass(scoped_ptr<RenderPass> render_pass) OVERRIDE {
    render_passes_.push_back(render_pass.Pass());
  }

  const ScopedPtrVector<RenderPass>& RenderPasses() const {
    return render_passes_;
  }

 private:
  ScopedPtrVector<RenderPass> render_passes_;
};

TEST(RenderSurfaceTest, SanityCheckSurfaceCreatesCorrectRenderPass) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  scoped_ptr<LayerImpl> root_layer =
      LayerImpl::Create(host_impl.active_tree(), 1);

  scoped_ptr<LayerImpl> owning_layer =
      LayerImpl::Create(host_impl.active_tree(), 2);
  owning_layer->CreateRenderSurface();
  ASSERT_TRUE(owning_layer->render_surface());
  owning_layer->draw_properties().render_target = owning_layer.get();
  RenderSurfaceImpl* render_surface = owning_layer->render_surface();

  root_layer->AddChild(owning_layer.Pass());

  gfx::Rect content_rect(0, 0, 50, 50);
  gfx::Transform origin;
  origin.Translate(30.0, 40.0);

  render_surface->SetScreenSpaceTransform(origin);
  render_surface->SetContentRect(content_rect);

  TestRenderPassSink pass_sink;

  render_surface->AppendRenderPasses(&pass_sink);

  ASSERT_EQ(1u, pass_sink.RenderPasses().size());
  RenderPass* pass = pass_sink.RenderPasses()[0];

  EXPECT_EQ(RenderPass::Id(2, 0), pass->id);
  EXPECT_RECT_EQ(content_rect, pass->output_rect);
  EXPECT_EQ(origin, pass->transform_to_root_target);
}

}  // namespace
}  // namespace cc
