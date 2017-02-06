// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/picture_layer_tiling.h"

#include <stddef.h>

#include <limits>
#include <set>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/tiles/picture_layer_tiling_set.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {

static gfx::Rect ViewportInLayerSpace(
    const gfx::Transform& transform,
    const gfx::Size& device_viewport) {

  gfx::Transform inverse;
  if (!transform.GetInverse(&inverse))
    return gfx::Rect();

  return MathUtil::ProjectEnclosingClippedRect(inverse,
                                               gfx::Rect(device_viewport));
}

class TestablePictureLayerTiling : public PictureLayerTiling {
 public:
  using PictureLayerTiling::SetLiveTilesRect;
  using PictureLayerTiling::TileAt;

  static std::unique_ptr<TestablePictureLayerTiling> Create(
      WhichTree tree,
      float contents_scale,
      scoped_refptr<RasterSource> raster_source,
      PictureLayerTilingClient* client,
      const LayerTreeSettings& settings) {
    return base::WrapUnique(new TestablePictureLayerTiling(
        tree, contents_scale, raster_source, client,
        settings.tiling_interest_area_padding,
        settings.skewport_target_time_in_seconds,
        settings.skewport_extrapolation_limit_in_screen_pixels));
  }

  gfx::Rect live_tiles_rect() const { return live_tiles_rect_; }

  using PictureLayerTiling::RemoveTileAt;
  using PictureLayerTiling::RemoveTilesInRegion;

 protected:
  TestablePictureLayerTiling(WhichTree tree,
                             float contents_scale,
                             scoped_refptr<RasterSource> raster_source,
                             PictureLayerTilingClient* client,
                             size_t tiling_interest_area_padding,
                             float skewport_target_time,
                             int skewport_extrapolation_limit)
      : PictureLayerTiling(tree, contents_scale, raster_source, client) {}
};

class PictureLayerTilingIteratorTest : public testing::Test {
 public:
  PictureLayerTilingIteratorTest() {}
  ~PictureLayerTilingIteratorTest() override {}

  void Initialize(const gfx::Size& tile_size,
                  float contents_scale,
                  const gfx::Size& layer_bounds) {
    client_.SetTileSize(tile_size);
    scoped_refptr<FakeRasterSource> raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);
    tiling_ = TestablePictureLayerTiling::Create(PENDING_TREE, contents_scale,
                                                 raster_source, &client_,
                                                 LayerTreeSettings());
    tiling_->set_resolution(HIGH_RESOLUTION);
  }

  void InitializeActive(const gfx::Size& tile_size,
                        float contents_scale,
                        const gfx::Size& layer_bounds) {
    client_.SetTileSize(tile_size);
    scoped_refptr<FakeRasterSource> raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);
    tiling_ = TestablePictureLayerTiling::Create(ACTIVE_TREE, contents_scale,
                                                 raster_source, &client_,
                                                 LayerTreeSettings());
    tiling_->set_resolution(HIGH_RESOLUTION);
  }

  void SetLiveRectAndVerifyTiles(const gfx::Rect& live_tiles_rect) {
    tiling_->SetLiveTilesRect(live_tiles_rect);

    std::vector<Tile*> tiles = tiling_->AllTilesForTesting();
    for (std::vector<Tile*>::iterator iter = tiles.begin();
         iter != tiles.end();
         ++iter) {
      EXPECT_TRUE(live_tiles_rect.Intersects((*iter)->content_rect()));
    }
  }

  void VerifyTilesExactlyCoverRect(
      float rect_scale,
      const gfx::Rect& request_rect,
      const gfx::Rect& expect_rect) {
    EXPECT_TRUE(request_rect.Contains(expect_rect));

    // Iterators are not valid if this ratio is too large (i.e. the
    // tiling is too high-res for a low-res destination rect.)  This is an
    // artifact of snapping geometry to integer coordinates and then mapping
    // back to floating point texture coordinates.
    float dest_to_contents_scale = tiling_->contents_scale() / rect_scale;
    ASSERT_LE(dest_to_contents_scale, 2.0);

    Region remaining = expect_rect;
    for (PictureLayerTiling::CoverageIterator
             iter(tiling_.get(), rect_scale, request_rect);
         iter;
         ++iter) {
      // Geometry cannot overlap previous geometry at all
      gfx::Rect geometry = iter.geometry_rect();
      EXPECT_TRUE(expect_rect.Contains(geometry));
      EXPECT_TRUE(remaining.Contains(geometry));
      remaining.Subtract(geometry);

      // Sanity check that texture coords are within the texture rect.
      gfx::RectF texture_rect = iter.texture_rect();
      EXPECT_GE(texture_rect.x(), 0);
      EXPECT_GE(texture_rect.y(), 0);
      EXPECT_LE(texture_rect.right(), client_.TileSize().width());
      EXPECT_LE(texture_rect.bottom(), client_.TileSize().height());
    }

    // The entire rect must be filled by geometry from the tiling.
    EXPECT_TRUE(remaining.IsEmpty());
  }

  void VerifyTilesExactlyCoverRect(float rect_scale, const gfx::Rect& rect) {
    VerifyTilesExactlyCoverRect(rect_scale, rect, rect);
  }

  void VerifyTiles(
      float rect_scale,
      const gfx::Rect& rect,
      base::Callback<void(Tile* tile,
                          const gfx::Rect& geometry_rect)> callback) {
    VerifyTiles(tiling_.get(),
                rect_scale,
                rect,
                callback);
  }

  void VerifyTiles(
      PictureLayerTiling* tiling,
      float rect_scale,
      const gfx::Rect& rect,
      base::Callback<void(Tile* tile,
                          const gfx::Rect& geometry_rect)> callback) {
    Region remaining = rect;
    for (PictureLayerTiling::CoverageIterator iter(tiling, rect_scale, rect);
         iter;
         ++iter) {
      remaining.Subtract(iter.geometry_rect());
      callback.Run(*iter, iter.geometry_rect());
    }
    EXPECT_TRUE(remaining.IsEmpty());
  }

  void VerifyTilesCoverNonContainedRect(float rect_scale,
                                        const gfx::Rect& dest_rect) {
    float dest_to_contents_scale = tiling_->contents_scale() / rect_scale;
    gfx::Rect clamped_rect = gfx::ScaleToEnclosingRect(
        gfx::Rect(tiling_->tiling_size()), 1.f / dest_to_contents_scale);
    clamped_rect.Intersect(dest_rect);
    VerifyTilesExactlyCoverRect(rect_scale, dest_rect, clamped_rect);
  }

 protected:
  FakePictureLayerTilingClient client_;
  std::unique_ptr<TestablePictureLayerTiling> tiling_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureLayerTilingIteratorTest);
};

TEST_F(PictureLayerTilingIteratorTest, ResizeDeletesTiles) {
  // Verifies that a resize with invalidation for newly exposed pixels will
  // deletes tiles that intersect that invalidation.
  gfx::Size tile_size(100, 100);
  gfx::Size original_layer_size(10, 10);
  InitializeActive(tile_size, 1.f, original_layer_size);
  SetLiveRectAndVerifyTiles(gfx::Rect(original_layer_size));

  // Tiling only has one tile, since its total size is less than one.
  EXPECT_TRUE(tiling_->TileAt(0, 0));

  // Stop creating tiles so that any invalidations are left as holes.
  gfx::Size new_layer_size(200, 200);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreatePartiallyFilled(new_layer_size, gfx::Rect());

  Region invalidation =
      SubtractRegions(gfx::Rect(tile_size), gfx::Rect(original_layer_size));
  tiling_->SetRasterSourceAndResize(raster_source);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  tiling_->Invalidate(invalidation);
  EXPECT_FALSE(tiling_->TileAt(0, 0));
}

TEST_F(PictureLayerTilingIteratorTest, CreateMissingTilesStaysInsideLiveRect) {
  // The tiling has three rows and columns.
  Initialize(gfx::Size(100, 100), 1.f, gfx::Size(250, 250));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect is at the very edge of the right-most and
  // bottom-most tiles. Their border pixels would still be inside the live
  // tiles rect, but the tiles should not exist just for that.
  int right = tiling_->TilingDataForTesting().TileBounds(2, 2).x();
  int bottom = tiling_->TilingDataForTesting().TileBounds(2, 2).y();

  SetLiveRectAndVerifyTiles(gfx::Rect(right, bottom));
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(1, 2));
  EXPECT_FALSE(tiling_->TileAt(0, 2));

  // Verify CreateMissingTilesInLiveTilesRect respects this.
  tiling_->CreateMissingTilesInLiveTilesRect();
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(1, 2));
  EXPECT_FALSE(tiling_->TileAt(0, 2));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeTilingOverTileBorders) {
  // The tiling has four rows and three columns.
  Initialize(gfx::Size(100, 100), 1.f, gfx::Size(250, 350));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect covers the whole tiling.
  SetLiveRectAndVerifyTiles(gfx::Rect(250, 350));

  // Tiles in the bottom row and right column exist.
  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 3));

  int right = tiling_->TilingDataForTesting().TileBounds(2, 2).x();
  int bottom = tiling_->TilingDataForTesting().TileBounds(2, 3).y();

  // Shrink the tiling so that the last tile row/column is entirely in the
  // border pixels of the interior tiles. That row/column is removed.
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(right + 1, bottom + 1));
  tiling_->SetRasterSourceAndResize(raster_source);
  EXPECT_EQ(2, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect was clamped to the raster source size.
  EXPECT_EQ(gfx::Rect(right + 1, bottom + 1), tiling_->live_tiles_rect());

  // Since the row/column is gone, the tiles should be gone too.
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(2, 3));
  EXPECT_FALSE(tiling_->TileAt(1, 3));
  EXPECT_FALSE(tiling_->TileAt(0, 3));

  // Growing outside the current right/bottom tiles border pixels should create
  // the tiles again, even though the live rect has not changed size.
  raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(right + 2, bottom + 2));
  tiling_->SetRasterSourceAndResize(raster_source);
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // Not changed.
  EXPECT_EQ(gfx::Rect(right + 1, bottom + 1), tiling_->live_tiles_rect());

  // The last row/column tiles are inside the live tiles rect.
  EXPECT_TRUE(gfx::Rect(right + 1, bottom + 1).Intersects(
      tiling_->TilingDataForTesting().TileBounds(2, 0)));
  EXPECT_TRUE(gfx::Rect(right + 1, bottom + 1).Intersects(
      tiling_->TilingDataForTesting().TileBounds(0, 3)));

  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 3));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeLiveTileRectOverTileBorders) {
  // The tiling has three rows and columns.
  Initialize(gfx::Size(100, 100), 1.f, gfx::Size(250, 350));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect covers the whole tiling.
  SetLiveRectAndVerifyTiles(gfx::Rect(250, 350));

  // Tiles in the bottom row and right column exist.
  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 3));

  // Shrink the live tiles rect to the very edge of the right-most and
  // bottom-most tiles. Their border pixels would still be inside the live
  // tiles rect, but the tiles should not exist just for that.
  int right = tiling_->TilingDataForTesting().TileBounds(2, 3).x();
  int bottom = tiling_->TilingDataForTesting().TileBounds(2, 3).y();

  SetLiveRectAndVerifyTiles(gfx::Rect(right, bottom));
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(2, 3));
  EXPECT_FALSE(tiling_->TileAt(1, 3));
  EXPECT_FALSE(tiling_->TileAt(0, 3));

  // Including the bottom row and right column again, should create the tiles.
  SetLiveRectAndVerifyTiles(gfx::Rect(right + 1, bottom + 1));
  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 2));
  EXPECT_TRUE(tiling_->TileAt(0, 2));

  // Shrink the live tiles rect to the very edge of the left-most and
  // top-most tiles. Their border pixels would still be inside the live
  // tiles rect, but the tiles should not exist just for that.
  int left = tiling_->TilingDataForTesting().TileBounds(0, 0).right();
  int top = tiling_->TilingDataForTesting().TileBounds(0, 0).bottom();

  SetLiveRectAndVerifyTiles(gfx::Rect(left, top, 250 - left, 350 - top));
  EXPECT_FALSE(tiling_->TileAt(0, 3));
  EXPECT_FALSE(tiling_->TileAt(0, 2));
  EXPECT_FALSE(tiling_->TileAt(0, 1));
  EXPECT_FALSE(tiling_->TileAt(0, 0));
  EXPECT_FALSE(tiling_->TileAt(1, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 0));

  // Including the top row and left column again, should create the tiles.
  SetLiveRectAndVerifyTiles(
      gfx::Rect(left - 1, top - 1, 250 - left, 350 - top));
  EXPECT_TRUE(tiling_->TileAt(0, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 2));
  EXPECT_TRUE(tiling_->TileAt(0, 1));
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  EXPECT_TRUE(tiling_->TileAt(1, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 0));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeLiveTileRectOverSameTiles) {
  // The tiling has four rows and three columns.
  Initialize(gfx::Size(100, 100), 1.f, gfx::Size(250, 350));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect covers the whole tiling.
  SetLiveRectAndVerifyTiles(gfx::Rect(250, 350));

  // All tiles exist.
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      EXPECT_TRUE(tiling_->TileAt(i, j)) << i << "," << j;
  }

  // Shrink the live tiles rect, but still cover all the tiles.
  SetLiveRectAndVerifyTiles(gfx::Rect(1, 1, 249, 349));

  // All tiles still exist.
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      EXPECT_TRUE(tiling_->TileAt(i, j)) << i << "," << j;
  }

  // Grow the live tiles rect, but still cover all the same tiles.
  SetLiveRectAndVerifyTiles(gfx::Rect(0, 0, 250, 350));

  // All tiles still exist.
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      EXPECT_TRUE(tiling_->TileAt(i, j)) << i << "," << j;
  }
}

TEST_F(PictureLayerTilingIteratorTest, ResizeOverBorderPixelsDeletesTiles) {
  // Verifies that a resize with invalidation for newly exposed pixels will
  // deletes tiles that intersect that invalidation.
  gfx::Size tile_size(100, 100);
  gfx::Size original_layer_size(99, 99);
  InitializeActive(tile_size, 1.f, original_layer_size);
  SetLiveRectAndVerifyTiles(gfx::Rect(original_layer_size));

  // Tiling only has one tile, since its total size is less than one.
  EXPECT_TRUE(tiling_->TileAt(0, 0));

  // Stop creating tiles so that any invalidations are left as holes.
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreatePartiallyFilled(gfx::Size(200, 200), gfx::Rect());
  tiling_->SetRasterSourceAndResize(raster_source);

  Region invalidation =
      SubtractRegions(gfx::Rect(tile_size), gfx::Rect(original_layer_size));
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  tiling_->Invalidate(invalidation);
  EXPECT_FALSE(tiling_->TileAt(0, 0));

  // The original tile was the same size after resize, but it would include new
  // border pixels.
  EXPECT_EQ(gfx::Rect(original_layer_size),
            tiling_->TilingDataForTesting().TileBounds(0, 0));
}

TEST_F(PictureLayerTilingIteratorTest, RemoveOutsideLayerKeepsTiles) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_size(100, 100);
  InitializeActive(tile_size, 1.f, layer_size);
  SetLiveRectAndVerifyTiles(gfx::Rect(layer_size));

  // In all cases here, the tiling should remain with one tile, since the remove
  // region doesn't intersect it.

  bool recreate_tiles = false;
  // Top
  tiling_->RemoveTilesInRegion(gfx::Rect(50, -1, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  // Bottom
  tiling_->RemoveTilesInRegion(gfx::Rect(50, 100, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  // Left
  tiling_->RemoveTilesInRegion(gfx::Rect(-1, 50, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  // Right
  tiling_->RemoveTilesInRegion(gfx::Rect(100, 50, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
}

TEST_F(PictureLayerTilingIteratorTest, CreateTileJustCoverBorderUp) {
  float content_scale = 1.2000000476837158f;
  gfx::Size tile_size(512, 512);
  gfx::Size layer_size(1440, 4560);
  FakePictureLayerTilingClient active_client;

  active_client.SetTileSize(tile_size);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_size);
  std::unique_ptr<TestablePictureLayerTiling> active_tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, content_scale,
                                         raster_source, &active_client,
                                         LayerTreeSettings());
  active_tiling->set_resolution(HIGH_RESOLUTION);

  gfx::Rect invalid_rect(0, 750, 220, 100);
  Initialize(tile_size, content_scale, layer_size);
  client_.set_twin_tiling(active_tiling.get());
  client_.set_invalidation(invalid_rect);
  SetLiveRectAndVerifyTiles(gfx::Rect(layer_size));
  // When it creates a tile in pending tree, verify that tiles are invalidated
  // even if only their border pixels intersect the invalidation rect
  EXPECT_TRUE(tiling_->TileAt(0, 1));
  gfx::Rect scaled_invalid_rect =
      gfx::ScaleToEnclosingRect(invalid_rect, content_scale);
  EXPECT_FALSE(scaled_invalid_rect.Intersects(
      tiling_->TilingDataForTesting().TileBounds(0, 2)));
  EXPECT_TRUE(scaled_invalid_rect.Intersects(
      tiling_->TilingDataForTesting().TileBoundsWithBorder(0, 2)));
  EXPECT_TRUE(tiling_->TileAt(0, 2));

  bool recreate_tiles = false;
  active_tiling->RemoveTilesInRegion(invalid_rect, recreate_tiles);
  // Even though a tile just touch border area of invalid region, verify that
  // RemoveTilesInRegion behaves the same as SetLiveRectAndVerifyTiles with
  // respect to the tiles that it invalidates
  EXPECT_FALSE(active_tiling->TileAt(0, 1));
  EXPECT_FALSE(active_tiling->TileAt(0, 2));
}

TEST_F(PictureLayerTilingIteratorTest, LiveTilesExactlyCoverLiveTileRect) {
  Initialize(gfx::Size(100, 100), 1.f, gfx::Size(1099, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(100, 100));
  SetLiveRectAndVerifyTiles(gfx::Rect(101, 99));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(1, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(201, 800));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsNoScale) {
  Initialize(gfx::Size(100, 100), 1.f, gfx::Size(1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(52, 83, 789, 412));

  // With borders, a size of 3x3 = 1 pixel of content.
  Initialize(gfx::Size(3, 3), 1.f, gfx::Size(10, 10));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1, 1));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(1, 1, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(3, 2, 5, 2));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsTilingScale) {
  Initialize(gfx::Size(200, 100), 2.0f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  Initialize(gfx::Size(3, 3), 2.0f, gfx::Size(10, 10));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1, 1));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(1, 1, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(3, 2, 5, 2));

  Initialize(gfx::Size(100, 200), 0.5f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  Initialize(gfx::Size(150, 250), 0.37f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  Initialize(gfx::Size(312, 123), 0.01f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsBothScale) {
  Initialize(gfx::Size(50, 50), 4.0f, gfx::Size(800, 600));
  VerifyTilesExactlyCoverRect(2.0f, gfx::Rect());
  VerifyTilesExactlyCoverRect(2.0f, gfx::Rect(0, 0, 1600, 1200));
  VerifyTilesExactlyCoverRect(2.0f, gfx::Rect(512, 365, 253, 182));

  float scale = 6.7f;
  gfx::Size bounds(800, 600);
  gfx::Rect full_rect(gfx::ScaleToCeiledSize(bounds, scale));
  Initialize(gfx::Size(256, 512), 5.2f, bounds);
  VerifyTilesExactlyCoverRect(scale, full_rect);
  VerifyTilesExactlyCoverRect(scale, gfx::Rect(2014, 1579, 867, 1033));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorEmptyRect) {
  Initialize(gfx::Size(100, 100), 1.0f, gfx::Size(800, 600));

  gfx::Rect empty;
  PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1.0f, empty);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, NonIntersectingRect) {
  Initialize(gfx::Size(100, 100), 1.0f, gfx::Size(800, 600));
  gfx::Rect non_intersecting(1000, 1000, 50, 50);
  PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1, non_intersecting);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, LayerEdgeTextureCoordinates) {
  Initialize(gfx::Size(300, 300), 1.0f, gfx::Size(256, 256));
  // All of these sizes are 256x256, scaled and ceiled.
  VerifyTilesExactlyCoverRect(1.0f, gfx::Rect(0, 0, 256, 256));
  VerifyTilesExactlyCoverRect(0.8f, gfx::Rect(0, 0, 205, 205));
  VerifyTilesExactlyCoverRect(1.2f, gfx::Rect(0, 0, 308, 308));
}

TEST_F(PictureLayerTilingIteratorTest, NonContainedDestRect) {
  Initialize(gfx::Size(100, 100), 1.0f, gfx::Size(400, 400));

  // Too large in all dimensions
  VerifyTilesCoverNonContainedRect(1.0f, gfx::Rect(-1000, -1000, 2000, 2000));
  VerifyTilesCoverNonContainedRect(1.5f, gfx::Rect(-1000, -1000, 2000, 2000));
  VerifyTilesCoverNonContainedRect(0.5f, gfx::Rect(-1000, -1000, 2000, 2000));

  // Partially covering content, but too large
  VerifyTilesCoverNonContainedRect(1.0f, gfx::Rect(-1000, 100, 2000, 100));
  VerifyTilesCoverNonContainedRect(1.5f, gfx::Rect(-1000, 100, 2000, 100));
  VerifyTilesCoverNonContainedRect(0.5f, gfx::Rect(-1000, 100, 2000, 100));
}

static void TileExists(bool exists, Tile* tile,
                       const gfx::Rect& geometry_rect) {
  EXPECT_EQ(exists, tile != NULL) << geometry_rect.ToString();
}

TEST_F(PictureLayerTilingIteratorTest, TilesExist) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  tiling_->ComputeTilePriorityRects(
      gfx::Rect(layer_bounds),  // visible rect
      gfx::Rect(layer_bounds),  // skewport
      gfx::Rect(layer_bounds),  // soon border rect
      gfx::Rect(layer_bounds),  // eventually rect
      1.f,                      // current contents scale
      Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));

  // Make the viewport rect empty. All tiles are killed and become zombies.
  tiling_->ComputeTilePriorityRects(gfx::Rect(), gfx::Rect(), gfx::Rect(),
                                    gfx::Rect(), 1.f, Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));
}

TEST_F(PictureLayerTilingIteratorTest, TilesExistGiantViewport) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  gfx::Rect giant_rect(-10000000, -10000000, 1000000000, 1000000000);

  tiling_->ComputeTilePriorityRects(
      gfx::Rect(layer_bounds),  // visible rect
      gfx::Rect(layer_bounds),  // skewport
      gfx::Rect(layer_bounds),  // soon border rect
      gfx::Rect(layer_bounds),  // eventually rect
      1.f,                      // current contents scale
      Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));

  // If the visible content rect is huge, we should still have live tiles.
  tiling_->ComputeTilePriorityRects(giant_rect, giant_rect, giant_rect,
                                    giant_rect, 1.f, Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));
}

TEST_F(PictureLayerTilingIteratorTest, TilesExistOutsideViewport) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  // This rect does not intersect with the layer, as the layer is outside the
  // viewport.
  gfx::Rect viewport_rect(1100, 0, 1000, 1000);
  EXPECT_FALSE(viewport_rect.Intersects(gfx::Rect(layer_bounds)));

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_rect;
  eventually_rect.Inset(-settings.tiling_interest_area_padding,
                        -settings.tiling_interest_area_padding);
  tiling_->ComputeTilePriorityRects(viewport_rect, viewport_rect, viewport_rect,
                                    eventually_rect, 1.f, Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));
}

static void TilesIntersectingRectExist(const gfx::Rect& rect,
                                       bool intersect_exists,
                                       Tile* tile,
                                       const gfx::Rect& geometry_rect) {
  bool intersects = rect.Intersects(geometry_rect);
  bool expected_exists = intersect_exists ? intersects : !intersects;
  EXPECT_EQ(expected_exists, tile != NULL)
      << "Rects intersecting " << rect.ToString() << " should exist. "
      << "Current tile rect is " << geometry_rect.ToString();
}

TEST_F(PictureLayerTilingIteratorTest,
       TilesExistLargeViewportAndLayerWithSmallVisibleArea) {
  gfx::Size layer_bounds(10000, 10000);
  client_.SetTileSize(gfx::Size(100, 100));
  LayerTreeSettings settings;
  settings.tiling_interest_area_padding = 1;

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  tiling_ = TestablePictureLayerTiling::Create(PENDING_TREE, 1.f, raster_source,
                                               &client_, settings);
  tiling_->set_resolution(HIGH_RESOLUTION);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  gfx::Rect visible_rect(8000, 8000, 50, 50);

  tiling_->ComputeTilePriorityRects(visible_rect,  // visible rect
                                    visible_rect,  // skewport
                                    visible_rect,  // soon border rect
                                    visible_rect,  // eventually rect
                                    1.f,           // current contents scale
                                    Occlusion());
  VerifyTiles(1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TilesIntersectingRectExist, visible_rect, true));
}

TEST(ComputeTilePriorityRectsTest, VisibleTiles) {
  // The TilePriority of visible tiles should have zero distance_to_visible
  // and time_to_visible.
  FakePictureLayerTilingClient client;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(current_layer_bounds);
  std::unique_ptr<TestablePictureLayerTiling> tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, 1.0f, raster_source,
                                         &client, LayerTreeSettings());
  tiling->set_resolution(HIGH_RESOLUTION);

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_in_layer_space;
  eventually_rect.Inset(-settings.tiling_interest_area_padding,
                        -settings.tiling_interest_area_padding);
  tiling->ComputeTilePriorityRects(
      viewport_in_layer_space, viewport_in_layer_space, viewport_in_layer_space,
      eventually_rect, current_layer_contents_scale, Occlusion());
  auto prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);
}

TEST(ComputeTilePriorityRectsTest, OffscreenTiles) {
  // The TilePriority of offscreen tiles (without movement) should have nonzero
  // distance_to_visible and infinite time_to_visible.
  FakePictureLayerTilingClient client;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;

  current_screen_transform.Translate(850, 0);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(current_layer_bounds);
  std::unique_ptr<TestablePictureLayerTiling> tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, 1.0f, raster_source,
                                         &client, LayerTreeSettings());
  tiling->set_resolution(HIGH_RESOLUTION);

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_in_layer_space;
  eventually_rect.Inset(-settings.tiling_interest_area_padding,
                        -settings.tiling_interest_area_padding);
  tiling->ComputeTilePriorityRects(
      viewport_in_layer_space, viewport_in_layer_space, viewport_in_layer_space,
      eventually_rect, current_layer_contents_scale, Occlusion());
  auto prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  // Furthermore, in this scenario tiles on the right hand side should have a
  // larger distance to visible.
  TilePriority left = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  TilePriority right = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_GT(right.distance_to_visible, left.distance_to_visible);

  left = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  right = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_GT(right.distance_to_visible, left.distance_to_visible);
}

TEST(ComputeTilePriorityRectsTest, PartiallyOffscreenLayer) {
  // Sanity check that a layer with some tiles visible and others offscreen has
  // correct TilePriorities for each tile.
  FakePictureLayerTilingClient client;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;

  current_screen_transform.Translate(705, 505);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(current_layer_bounds);
  std::unique_ptr<TestablePictureLayerTiling> tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, 1.0f, raster_source,
                                         &client, LayerTreeSettings());
  tiling->set_resolution(HIGH_RESOLUTION);

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_in_layer_space;
  eventually_rect.Inset(-settings.tiling_interest_area_padding,
                        -settings.tiling_interest_area_padding);
  tiling->ComputeTilePriorityRects(
      viewport_in_layer_space, viewport_in_layer_space, viewport_in_layer_space,
      eventually_rect, current_layer_contents_scale, Occlusion());
  auto prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);
}

TEST(PictureLayerTilingTest, RecycledTilesClearedOnReset) {
  FakePictureLayerTilingClient active_client;
  active_client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(100, 100));
  std::unique_ptr<TestablePictureLayerTiling> active_tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, 1.0f, raster_source,
                                         &active_client, LayerTreeSettings());
  active_tiling->set_resolution(HIGH_RESOLUTION);
  // Create all tiles on this tiling.
  gfx::Rect visible_rect = gfx::Rect(0, 0, 100, 100);
  active_tiling->ComputeTilePriorityRects(
      visible_rect, visible_rect, visible_rect, visible_rect, 1.f, Occlusion());

  FakePictureLayerTilingClient recycle_client;
  recycle_client.SetTileSize(gfx::Size(100, 100));
  recycle_client.set_twin_tiling(active_tiling.get());

  LayerTreeSettings settings;

  raster_source = FakeRasterSource::CreateFilled(gfx::Size(100, 100));
  std::unique_ptr<TestablePictureLayerTiling> recycle_tiling =
      TestablePictureLayerTiling::Create(PENDING_TREE, 1.0f, raster_source,
                                         &recycle_client, settings);
  recycle_tiling->set_resolution(HIGH_RESOLUTION);

  // Create all tiles on the recycle tiling.
  recycle_tiling->ComputeTilePriorityRects(visible_rect, visible_rect,
                                           visible_rect, visible_rect, 1.0f,
                                           Occlusion());

  // Set the second tiling as recycled.
  active_client.set_twin_tiling(NULL);
  recycle_client.set_twin_tiling(NULL);

  EXPECT_TRUE(active_tiling->TileAt(0, 0));
  EXPECT_FALSE(recycle_tiling->TileAt(0, 0));

  // Reset the active tiling. The recycle tiles should be released too.
  active_tiling->Reset();
  EXPECT_FALSE(active_tiling->TileAt(0, 0));
  EXPECT_FALSE(recycle_tiling->TileAt(0, 0));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeTilesAndUpdateToCurrent) {
  // The tiling has four rows and three columns.
  Initialize(gfx::Size(150, 100), 1.f, gfx::Size(250, 150));
  tiling_->CreateAllTilesForTesting();
  EXPECT_EQ(150, tiling_->TilingDataForTesting().max_texture_size().width());
  EXPECT_EQ(100, tiling_->TilingDataForTesting().max_texture_size().height());
  EXPECT_EQ(4u, tiling_->AllTilesForTesting().size());

  client_.SetTileSize(gfx::Size(250, 200));

  // Tile size in the tiling should still be 150x100.
  EXPECT_EQ(150, tiling_->TilingDataForTesting().max_texture_size().width());
  EXPECT_EQ(100, tiling_->TilingDataForTesting().max_texture_size().height());

  // The layer's size isn't changed, but the tile size was.
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(250, 150));
  tiling_->SetRasterSourceAndResize(raster_source);

  // Tile size in the tiling should be resized to 250x200.
  EXPECT_EQ(250, tiling_->TilingDataForTesting().max_texture_size().width());
  EXPECT_EQ(200, tiling_->TilingDataForTesting().max_texture_size().height());
  EXPECT_EQ(0u, tiling_->AllTilesForTesting().size());
}

// This test runs into floating point issues because of big numbers.
TEST_F(PictureLayerTilingIteratorTest, GiantRect) {
  gfx::Size tile_size(256, 256);
  gfx::Size layer_size(33554432, 33554432);
  float contents_scale = 1.f;

  client_.SetTileSize(tile_size);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateEmpty(layer_size);
  tiling_ = TestablePictureLayerTiling::Create(PENDING_TREE, contents_scale,
                                               raster_source, &client_,
                                               LayerTreeSettings());

  gfx::Rect content_rect(25554432, 25554432, 950, 860);
  VerifyTilesExactlyCoverRect(contents_scale, content_rect);
}

}  // namespace
}  // namespace cc
