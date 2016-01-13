// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tiled_layer_impl.h"

#include "cc/layers/append_quads_data.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TiledLayerImplTest : public testing::Test {
 public:
  TiledLayerImplTest() : host_impl_(&proxy_, &shared_bitmap_manager_) {}

  scoped_ptr<TiledLayerImpl> CreateLayerNoTiles(
      const gfx::Size& tile_size,
      const gfx::Size& layer_size,
      LayerTilingData::BorderTexelOption border_texels) {
    scoped_ptr<TiledLayerImpl> layer =
        TiledLayerImpl::Create(host_impl_.active_tree(), 1);
    scoped_ptr<LayerTilingData> tiler =
        LayerTilingData::Create(tile_size, border_texels);
    tiler->SetTilingRect(gfx::Rect(layer_size));
    layer->SetTilingData(*tiler);
    layer->set_skips_draw(false);
    layer->draw_properties().visible_content_rect =
        gfx::Rect(layer_size);
    layer->draw_properties().opacity = 1;
    layer->SetBounds(layer_size);
    layer->SetContentBounds(layer_size);
    layer->CreateRenderSurface();
    layer->draw_properties().render_target = layer.get();
    return layer.Pass();
  }

  // Create a default tiled layer with textures for all tiles and a default
  // visibility of the entire layer size.
  scoped_ptr<TiledLayerImpl> CreateLayer(
      const gfx::Size& tile_size,
      const gfx::Size& layer_size,
      LayerTilingData::BorderTexelOption border_texels) {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayerNoTiles(tile_size, layer_size, border_texels);

    ResourceProvider::ResourceId resource_id = 1;
    for (int i = 0; i < layer->TilingForTesting()->num_tiles_x(); ++i) {
      for (int j = 0; j < layer->TilingForTesting()->num_tiles_y(); ++j) {
        gfx::Rect opaque_rect(
            layer->TilingForTesting()->tile_bounds(i, j).origin(),
            gfx::Size(1, 1));
        layer->PushTileProperties(i, j, resource_id++, opaque_rect, false);
      }
    }

    return layer.Pass();
  }

  void GetQuads(RenderPass* render_pass,
                const gfx::Size& tile_size,
                const gfx::Size& layer_size,
                LayerTilingData::BorderTexelOption border_texel_option,
                const gfx::Rect& visible_content_rect) {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, border_texel_option);
    layer->draw_properties().visible_content_rect = visible_content_rect;
    layer->SetBounds(layer_size);

    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    MockQuadCuller quad_culler(render_pass, &occlusion_tracker);
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
  }

 protected:
  FakeImplProxy proxy_;
  TestSharedBitmapManager shared_bitmap_manager_;
  FakeLayerTreeHostImpl host_impl_;
};

TEST_F(TiledLayerImplTest, EmptyQuadList) {
  gfx::Size tile_size(90, 90);
  int num_tiles_x = 8;
  int num_tiles_y = 4;
  gfx::Size layer_size(tile_size.width() * num_tiles_x,
                       tile_size.height() * num_tiles_y);

  // Verify default layer does creates quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);
    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    AppendQuadsData data;
    EXPECT_TRUE(layer->WillDraw(DRAW_MODE_HARDWARE, NULL));
    layer->AppendQuads(&quad_culler, &data);
    layer->DidDraw(NULL);
    unsigned num_tiles = num_tiles_x * num_tiles_y;
    EXPECT_EQ(quad_culler.quad_list().size(), num_tiles);
  }

  // Layer with empty visible layer rect produces no quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);
    layer->draw_properties().visible_content_rect = gfx::Rect();

    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    EXPECT_FALSE(layer->WillDraw(DRAW_MODE_HARDWARE, NULL));
  }

  // Layer with non-intersecting visible layer rect produces no quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);

    gfx::Rect outside_bounds(-100, -100, 50, 50);
    layer->draw_properties().visible_content_rect = outside_bounds;

    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    AppendQuadsData data;
    EXPECT_TRUE(layer->WillDraw(DRAW_MODE_HARDWARE, NULL));
    layer->AppendQuads(&quad_culler, &data);
    layer->DidDraw(NULL);
    EXPECT_EQ(quad_culler.quad_list().size(), 0u);
  }

  // Layer with skips draw produces no quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);
    layer->set_skips_draw(true);

    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_EQ(quad_culler.quad_list().size(), 0u);
  }
}

TEST_F(TiledLayerImplTest, Checkerboarding) {
  gfx::Size tile_size(10, 10);
  int num_tiles_x = 2;
  int num_tiles_y = 2;
  gfx::Size layer_size(tile_size.width() * num_tiles_x,
                       tile_size.height() * num_tiles_y);

  scoped_ptr<TiledLayerImpl> layer =
      CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);

  // No checkerboarding
  {
    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_EQ(quad_culler.quad_list().size(), 4u);
    EXPECT_EQ(0u, data.num_missing_tiles);

    for (size_t i = 0; i < quad_culler.quad_list().size(); ++i)
      EXPECT_EQ(quad_culler.quad_list()[i]->material, DrawQuad::TILED_CONTENT);
  }

  for (int i = 0; i < num_tiles_x; ++i)
    for (int j = 0; j < num_tiles_y; ++j)
      layer->PushTileProperties(i, j, 0, gfx::Rect(), false);

  // All checkerboarding
  {
    MockOcclusionTracker<LayerImpl> occlusion_tracker;
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    MockQuadCuller quad_culler(render_pass.get(), &occlusion_tracker);

    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_LT(0u, data.num_missing_tiles);
    EXPECT_EQ(quad_culler.quad_list().size(), 4u);
    for (size_t i = 0; i < quad_culler.quad_list().size(); ++i)
      EXPECT_NE(quad_culler.quad_list()[i]->material, DrawQuad::TILED_CONTENT);
  }
}

// Test with both border texels and without.
#define WITH_AND_WITHOUT_BORDER_TEST(text_fixture_name)                        \
  TEST_F(TiledLayerImplBorderTest, text_fixture_name##NoBorders) {             \
    text_fixture_name(LayerTilingData::NO_BORDER_TEXELS);                      \
  }                                                                            \
  TEST_F(TiledLayerImplBorderTest, text_fixture_name##HasBorders) {            \
    text_fixture_name(LayerTilingData::HAS_BORDER_TEXELS);                     \
  }

class TiledLayerImplBorderTest : public TiledLayerImplTest {
 public:
  void CoverageVisibleRectOnTileBoundaries(
      LayerTilingData::BorderTexelOption borders) {
    gfx::Size layer_size(1000, 1000);
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    GetQuads(render_pass.get(),
             gfx::Size(100, 100),
             layer_size,
             borders,
             gfx::Rect(layer_size));
    LayerTestCommon::VerifyQuadsExactlyCoverRect(render_pass->quad_list,
                                                 gfx::Rect(layer_size));
  }

  void CoverageVisibleRectIntersectsTiles(
      LayerTilingData::BorderTexelOption borders) {
    // This rect intersects the middle 3x3 of the 5x5 tiles.
    gfx::Point top_left(65, 73);
    gfx::Point bottom_right(182, 198);
    gfx::Rect visible_content_rect = gfx::BoundingRect(top_left, bottom_right);

    gfx::Size layer_size(250, 250);
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    GetQuads(render_pass.get(),
             gfx::Size(50, 50),
             gfx::Size(250, 250),
             LayerTilingData::NO_BORDER_TEXELS,
             visible_content_rect);
    LayerTestCommon::VerifyQuadsExactlyCoverRect(render_pass->quad_list,
                                                 visible_content_rect);
  }

  void CoverageVisibleRectIntersectsBounds(
      LayerTilingData::BorderTexelOption borders) {
    gfx::Size layer_size(220, 210);
    gfx::Rect visible_content_rect(layer_size);
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    GetQuads(render_pass.get(),
             gfx::Size(100, 100),
             layer_size,
             LayerTilingData::NO_BORDER_TEXELS,
             visible_content_rect);
    LayerTestCommon::VerifyQuadsExactlyCoverRect(render_pass->quad_list,
                                                 visible_content_rect);
  }
};
WITH_AND_WITHOUT_BORDER_TEST(CoverageVisibleRectOnTileBoundaries);

WITH_AND_WITHOUT_BORDER_TEST(CoverageVisibleRectIntersectsTiles);

WITH_AND_WITHOUT_BORDER_TEST(CoverageVisibleRectIntersectsBounds);

TEST_F(TiledLayerImplTest, TextureInfoForLayerNoBorders) {
  gfx::Size tile_size(50, 50);
  gfx::Size layer_size(250, 250);
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  GetQuads(render_pass.get(),
           tile_size,
           layer_size,
           LayerTilingData::NO_BORDER_TEXELS,
           gfx::Rect(layer_size));

  for (size_t i = 0; i < render_pass->quad_list.size(); ++i) {
    const TileDrawQuad* quad =
        TileDrawQuad::MaterialCast(render_pass->quad_list[i]);

    EXPECT_NE(0u, quad->resource_id) << LayerTestCommon::quad_string << i;
    EXPECT_EQ(gfx::RectF(gfx::PointF(), tile_size), quad->tex_coord_rect)
        << LayerTestCommon::quad_string << i;
    EXPECT_EQ(tile_size, quad->texture_size) << LayerTestCommon::quad_string
                                             << i;
    EXPECT_EQ(gfx::Size(1, 1).ToString(), quad->opaque_rect.size().ToString())
        << LayerTestCommon::quad_string << i;
  }
}

TEST_F(TiledLayerImplTest, GPUMemoryUsage) {
  gfx::Size tile_size(20, 30);
  int num_tiles_x = 12;
  int num_tiles_y = 32;
  gfx::Size layer_size(tile_size.width() * num_tiles_x,
                       tile_size.height() * num_tiles_y);

  scoped_ptr<TiledLayerImpl> layer = CreateLayerNoTiles(
      tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);

  EXPECT_EQ(layer->GPUMemoryUsageInBytes(), 0u);

  ResourceProvider::ResourceId resource_id = 1;
  layer->PushTileProperties(0, 1, resource_id++, gfx::Rect(0, 0, 1, 1), false);
  layer->PushTileProperties(2, 3, resource_id++, gfx::Rect(0, 0, 1, 1), false);
  layer->PushTileProperties(2, 0, resource_id++, gfx::Rect(0, 0, 1, 1), false);

  EXPECT_EQ(
      layer->GPUMemoryUsageInBytes(),
      static_cast<size_t>(3 * 4 * tile_size.width() * tile_size.height()));

  ResourceProvider::ResourceId empty_resource(0);
  layer->PushTileProperties(0, 1, empty_resource, gfx::Rect(0, 0, 1, 1), false);
  layer->PushTileProperties(2, 3, empty_resource, gfx::Rect(0, 0, 1, 1), false);
  layer->PushTileProperties(2, 0, empty_resource, gfx::Rect(0, 0, 1, 1), false);

  EXPECT_EQ(layer->GPUMemoryUsageInBytes(), 0u);
}

TEST_F(TiledLayerImplTest, EmptyMask) {
  gfx::Size tile_size(20, 20);
  gfx::Size layer_size(0, 0);
  scoped_ptr<TiledLayerImpl> layer =
      CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);

  EXPECT_EQ(0u, layer->ContentsResourceId());
  EXPECT_EQ(0, layer->TilingForTesting()->num_tiles_x());
  EXPECT_EQ(0, layer->TilingForTesting()->num_tiles_y());
}

TEST_F(TiledLayerImplTest, Occlusion) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;

  TiledLayerImpl* tiled_layer = impl.AddChildToRoot<TiledLayerImpl>();
  tiled_layer->SetBounds(layer_bounds);
  tiled_layer->SetContentBounds(layer_bounds);
  tiled_layer->SetDrawsContent(true);
  tiled_layer->set_skips_draw(false);

  scoped_ptr<LayerTilingData> tiler =
      LayerTilingData::Create(tile_size, LayerTilingData::NO_BORDER_TEXELS);
  tiler->SetTilingRect(gfx::Rect(layer_bounds));
  tiled_layer->SetTilingData(*tiler);

  ResourceProvider::ResourceId resource_id = 1;
  for (int i = 0; i < tiled_layer->TilingForTesting()->num_tiles_x(); ++i) {
    for (int j = 0; j < tiled_layer->TilingForTesting()->num_tiles_y(); ++j)
      tiled_layer->PushTileProperties(i, j, resource_id++, gfx::Rect(), false);
  }

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(tiled_layer, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_bounds));
    EXPECT_EQ(100u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(tiled_layer->visible_content_rect());
    impl.AppendQuadsWithOcclusion(tiled_layer, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(150, 0, 200, 1000);
    impl.AppendQuadsWithOcclusion(tiled_layer, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
        impl.quad_list(),
        gfx::Rect(layer_bounds),
        occluded,
        &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(100u - 10u, impl.quad_list().size());
    EXPECT_EQ(10u + 10u, partially_occluded_count);
  }
}

}  // namespace
}  // namespace cc
