// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <vector>

#include "cc/layers/append_quads_data.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SolidColorLayerImplTest, VerifyTilingCompleteAndNoOverlap) {
  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

  gfx::Size layer_size = gfx::Size(800, 600);
  gfx::Rect visible_content_rect = gfx::Rect(layer_size);

  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  scoped_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  LayerTestCommon::VerifyQuadsExactlyCoverRect(quad_culler.quad_list(),
                                               visible_content_rect);
}

TEST(SolidColorLayerImplTest, VerifyCorrectBackgroundColorInQuad) {
  SkColor test_color = 0xFFA55AFF;

  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_content_rect = gfx::Rect(layer_size);

  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  scoped_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->SetBackgroundColor(test_color);
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  ASSERT_EQ(quad_culler.quad_list().size(), 1U);
  EXPECT_EQ(SolidColorDrawQuad::MaterialCast(quad_culler.quad_list()[0])->color,
            test_color);
}

TEST(SolidColorLayerImplTest, VerifyCorrectOpacityInQuad) {
  const float opacity = 0.5f;

  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_content_rect = gfx::Rect(layer_size);

  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  scoped_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->draw_properties().opacity = opacity;
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  ASSERT_EQ(quad_culler.quad_list().size(), 1U);
  EXPECT_EQ(opacity,
            SolidColorDrawQuad::MaterialCast(quad_culler.quad_list()[0])
                ->opacity());
}

TEST(SolidColorLayerImplTest, VerifyOpaqueRect) {
  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_content_rect = gfx::Rect(layer_size);

  scoped_refptr<SolidColorLayer> layer = SolidColorLayer::Create();
  layer->SetBounds(layer_size);
  layer->SetForceRenderSurface(true);

  scoped_refptr<Layer> root = Layer::Create();
  root->AddChild(layer);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root, gfx::Size(500, 500), &render_surface_layer_list);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_FALSE(layer->contents_opaque());
  layer->SetBackgroundColor(SkColorSetARGBInline(255, 10, 20, 30));
  EXPECT_TRUE(layer->contents_opaque());
  {
    scoped_ptr<SolidColorLayerImpl> layer_impl =
        SolidColorLayerImpl::Create(host->host_impl()->active_tree(),
                                    layer->id());
    layer->PushPropertiesTo(layer_impl.get());

    // The impl layer should call itself opaque as well.
    EXPECT_TRUE(layer_impl->contents_opaque());

    // Impl layer has 1 opacity, and the color is opaque, so the opaque_rect
    // should be the full tile.
    layer_impl->draw_properties().opacity = 1;

    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    AppendQuadsData data;
    layer_impl->AppendQuads(&quad_culler, &data);

    ASSERT_EQ(quad_culler.quad_list().size(), 1U);
    EXPECT_EQ(visible_content_rect.ToString(),
              quad_culler.quad_list()[0]->opaque_rect.ToString());
  }

  EXPECT_TRUE(layer->contents_opaque());
  layer->SetBackgroundColor(SkColorSetARGBInline(254, 10, 20, 30));
  EXPECT_FALSE(layer->contents_opaque());
  {
    scoped_ptr<SolidColorLayerImpl> layer_impl =
        SolidColorLayerImpl::Create(host->host_impl()->active_tree(),
                                    layer->id());
    layer->PushPropertiesTo(layer_impl.get());

    // The impl layer should callnot itself opaque anymore.
    EXPECT_FALSE(layer_impl->contents_opaque());

    // Impl layer has 1 opacity, but the color is not opaque, so the opaque_rect
    // should be empty.
    layer_impl->draw_properties().opacity = 1;

    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    AppendQuadsData data;
    layer_impl->AppendQuads(&quad_culler, &data);

    ASSERT_EQ(quad_culler.quad_list().size(), 1U);
    EXPECT_EQ(gfx::Rect().ToString(),
              quad_culler.quad_list()[0]->opaque_rect.ToString());
  }
}

TEST(SolidColorLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;

  SolidColorLayerImpl* solid_color_layer_impl =
      impl.AddChildToRoot<SolidColorLayerImpl>();
  solid_color_layer_impl->SetBackgroundColor(SkColorSetARGB(255, 10, 20, 30));
  solid_color_layer_impl->SetBounds(layer_size);
  solid_color_layer_impl->SetContentBounds(layer_size);
  solid_color_layer_impl->SetDrawsContent(true);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_size));
    EXPECT_EQ(16u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(solid_color_layer_impl->visible_content_rect());
    impl.AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 200, 256 * 3, 256 * 3);
    impl.AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
        impl.quad_list(),
        gfx::Rect(layer_size),
        occluded,
        &partially_occluded_count);
    // 4 quads are completely occluded, 8 are partially occluded.
    EXPECT_EQ(16u - 4u, impl.quad_list().size());
    EXPECT_EQ(8u, partially_occluded_count);
  }
}

}  // namespace
}  // namespace cc
