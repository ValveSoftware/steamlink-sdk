// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/append_quads_data.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_ui_resource_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

scoped_ptr<UIResourceLayerImpl> GenerateUIResourceLayer(
    FakeUIResourceLayerTreeHostImpl* host_impl,
    const gfx::Size& bitmap_size,
    const gfx::Size& layer_size,
    bool opaque,
    UIResourceId uid) {
  gfx::Rect visible_content_rect(layer_size);
  scoped_ptr<UIResourceLayerImpl> layer =
      UIResourceLayerImpl::Create(host_impl->active_tree(), 1);
  layer->draw_properties().visible_content_rect = visible_content_rect;
  layer->SetBounds(layer_size);
  layer->SetContentBounds(layer_size);
  layer->CreateRenderSurface();
  layer->draw_properties().render_target = layer.get();

  UIResourceBitmap bitmap(bitmap_size, opaque);

  host_impl->CreateUIResource(uid, bitmap);
  layer->SetUIResourceId(uid);

  return layer.Pass();
}

void QuadSizeTest(scoped_ptr<UIResourceLayerImpl> layer,
                  size_t expected_quad_size) {
  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  // Verify quad rects
  const QuadList& quads = quad_culler.quad_list();
  EXPECT_EQ(expected_quad_size, quads.size());
}

TEST(UIResourceLayerImplTest, VerifyDrawQuads) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeUIResourceLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);
  // Make sure we're appending quads when there are valid values.
  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(100, 100);;
  size_t expected_quad_size = 1;
  bool opaque = true;
  UIResourceId uid = 1;
  scoped_ptr<UIResourceLayerImpl> layer = GenerateUIResourceLayer(&host_impl,
                                                                  bitmap_size,
                                                                  layer_size,
                                                                  opaque,
                                                                  uid);
  QuadSizeTest(layer.Pass(), expected_quad_size);

  // Make sure we're not appending quads when there are invalid values.
  expected_quad_size = 0;
  uid = 0;
  layer = GenerateUIResourceLayer(&host_impl,
                                  bitmap_size,
                                  layer_size,
                                  opaque,
                                  uid);
  QuadSizeTest(layer.Pass(), expected_quad_size);
}

void OpaqueBoundsTest(scoped_ptr<UIResourceLayerImpl> layer,
                 const gfx::Rect& expected_opaque_bounds) {
  MockOcclusionTracker<LayerImpl> occlusion_tracker;
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

  AppendQuadsData data;
  layer->AppendQuads(&quad_culler, &data);

  // Verify quad rects
  const QuadList& quads = quad_culler.quad_list();
  EXPECT_GE(quads.size(), (size_t)0);
  gfx::Rect opaque_rect = quads.at(0)->opaque_rect;
  EXPECT_EQ(expected_opaque_bounds, opaque_rect);
}

TEST(UIResourceLayerImplTest, VerifySetOpaqueOnSkBitmap) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeUIResourceLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);

  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(100, 100);;
  bool opaque = false;
  UIResourceId uid = 1;
  scoped_ptr<UIResourceLayerImpl> layer = GenerateUIResourceLayer(&host_impl,
                                                                  bitmap_size,
                                                                  layer_size,
                                                                  opaque,
                                                                  uid);
  gfx::Rect expected_opaque_bounds;
  OpaqueBoundsTest(layer.Pass(), expected_opaque_bounds);

  opaque = true;
  layer = GenerateUIResourceLayer(&host_impl,
                                  bitmap_size,
                                  layer_size,
                                  opaque,
                                  uid);
  expected_opaque_bounds = gfx::Rect(layer->bounds());
  OpaqueBoundsTest(layer.Pass(), expected_opaque_bounds);
}

TEST(UIResourceLayerImplTest, VerifySetOpaqueOnLayer) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  FakeUIResourceLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager);

  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(100, 100);
  bool skbitmap_opaque = false;
  UIResourceId uid = 1;
  scoped_ptr<UIResourceLayerImpl> layer = GenerateUIResourceLayer(
      &host_impl, bitmap_size, layer_size, skbitmap_opaque, uid);
  layer->SetContentsOpaque(false);
  gfx::Rect expected_opaque_bounds;
  OpaqueBoundsTest(layer.Pass(), expected_opaque_bounds);

  layer = GenerateUIResourceLayer(
      &host_impl, bitmap_size, layer_size, skbitmap_opaque, uid);
  layer->SetContentsOpaque(true);
  expected_opaque_bounds = gfx::Rect(layer->bounds());
  OpaqueBoundsTest(layer.Pass(), expected_opaque_bounds);
}

TEST(UIResourceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;

  SkBitmap sk_bitmap;
  sk_bitmap.allocN32Pixels(10, 10);
  sk_bitmap.setImmutable();
  UIResourceId uid = 5;
  UIResourceBitmap bitmap(sk_bitmap);
  impl.host_impl()->CreateUIResource(uid, bitmap);

  UIResourceLayerImpl* ui_resource_layer_impl =
      impl.AddChildToRoot<UIResourceLayerImpl>();
  ui_resource_layer_impl->SetBounds(layer_size);
  ui_resource_layer_impl->SetContentBounds(layer_size);
  ui_resource_layer_impl->SetDrawsContent(true);
  ui_resource_layer_impl->SetUIResourceId(uid);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(ui_resource_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(ui_resource_layer_impl->visible_content_rect());
    impl.AppendQuadsWithOcclusion(ui_resource_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(ui_resource_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
        impl.quad_list(),
        gfx::Rect(layer_size),
        occluded,
        &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

}  // namespace
}  // namespace cc
