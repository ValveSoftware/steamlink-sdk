// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling.h"

#include <limits>
#include <set>

#include "cc/base/math_util.h"
#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

static gfx::Rect ViewportInLayerSpace(
    const gfx::Transform& transform,
    const gfx::Size& device_viewport) {

  gfx::Transform inverse;
  if (!transform.GetInverse(&inverse))
    return gfx::Rect();

  gfx::RectF viewport_in_layer_space = MathUtil::ProjectClippedRect(
      inverse, gfx::RectF(gfx::Point(0, 0), device_viewport));
  return ToEnclosingRect(viewport_in_layer_space);
}

static void UpdateAllTilePriorities(PictureLayerTilingSet* set,
                                    WhichTree tree,
                                    const gfx::Rect& visible_layer_rect,
                                    float layer_contents_scale,
                                    double current_frame_time_in_seconds) {
  for (size_t i = 0; i < set->num_tilings(); ++i) {
    set->tiling_at(i)->UpdateTilePriorities(tree,
                                            visible_layer_rect,
                                            layer_contents_scale,
                                            current_frame_time_in_seconds);
  }
}

class TestablePictureLayerTiling : public PictureLayerTiling {
 public:
  using PictureLayerTiling::SetLiveTilesRect;
  using PictureLayerTiling::TileAt;

  static scoped_ptr<TestablePictureLayerTiling> Create(
      float contents_scale,
      const gfx::Size& layer_bounds,
      PictureLayerTilingClient* client) {
    return make_scoped_ptr(new TestablePictureLayerTiling(
        contents_scale,
        layer_bounds,
        client));
  }

  using PictureLayerTiling::ComputeSkewport;

 protected:
  TestablePictureLayerTiling(float contents_scale,
                             const gfx::Size& layer_bounds,
                             PictureLayerTilingClient* client)
      : PictureLayerTiling(contents_scale, layer_bounds, client) { }
};

class PictureLayerTilingIteratorTest : public testing::Test {
 public:
  PictureLayerTilingIteratorTest() {}
  virtual ~PictureLayerTilingIteratorTest() {}

  void Initialize(const gfx::Size& tile_size,
                  float contents_scale,
                  const gfx::Size& layer_bounds) {
    client_.SetTileSize(tile_size);
    tiling_ = TestablePictureLayerTiling::Create(contents_scale,
                                                 layer_bounds,
                                                 &client_);
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

      EXPECT_EQ(iter.texture_size(), client_.TileSize());
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
        tiling_->TilingRect(), 1.f / dest_to_contents_scale);
    clamped_rect.Intersect(dest_rect);
    VerifyTilesExactlyCoverRect(rect_scale, dest_rect, clamped_rect);
  }

  void set_max_tiles_for_interest_area(size_t area) {
    client_.set_max_tiles_for_interest_area(area);
  }

 protected:
  FakePictureLayerTilingClient client_;
  scoped_ptr<TestablePictureLayerTiling> tiling_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureLayerTilingIteratorTest);
};

TEST_F(PictureLayerTilingIteratorTest, ResizeDeletesTiles) {
  // Verifies that a resize deletes tiles that used to be on the edge.
  gfx::Size tile_size(100, 100);
  gfx::Size original_layer_size(10, 10);
  Initialize(tile_size, 1.f, original_layer_size);
  SetLiveRectAndVerifyTiles(gfx::Rect(original_layer_size));

  // Tiling only has one tile, since its total size is less than one.
  EXPECT_TRUE(tiling_->TileAt(0, 0));

  // Stop creating tiles so that any invalidations are left as holes.
  client_.set_allow_create_tile(false);

  tiling_->SetLayerBounds(gfx::Size(200, 200));
  EXPECT_FALSE(tiling_->TileAt(0, 0));
}

TEST_F(PictureLayerTilingIteratorTest, LiveTilesExactlyCoverLiveTileRect) {
  Initialize(gfx::Size(100, 100), 1, gfx::Size(1099, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(100, 100));
  SetLiveRectAndVerifyTiles(gfx::Rect(101, 99));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(1, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(201, 800));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsNoScale) {
  Initialize(gfx::Size(100, 100), 1, gfx::Size(1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(52, 83, 789, 412));

  // With borders, a size of 3x3 = 1 pixel of content.
  Initialize(gfx::Size(3, 3), 1, gfx::Size(10, 10));
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
  gfx::Rect full_rect(gfx::ToCeiledSize(gfx::ScaleSize(bounds, scale)));
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

TEST(PictureLayerTilingTest, SkewportLimits) {
  FakePictureLayerTilingClient client;
  client.set_skewport_extrapolation_limit_in_content_pixels(75);
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Rect viewport(0, 0, 100, 100);
  gfx::Size layer_bounds(200, 200);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f, layer_bounds, &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 1.f, 1.0);

  // Move viewport down 50 pixels in 0.5 seconds.
  gfx::Rect down_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(0, 50, 100, 100));

  EXPECT_EQ(0, down_skewport.x());
  EXPECT_EQ(50, down_skewport.y());
  EXPECT_EQ(100, down_skewport.width());
  EXPECT_EQ(175, down_skewport.height());
  EXPECT_TRUE(down_skewport.Contains(gfx::Rect(0, 50, 100, 100)));

  // Move viewport down 50 and right 10 pixels.
  gfx::Rect down_right_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(10, 50, 100, 100));

  EXPECT_EQ(10, down_right_skewport.x());
  EXPECT_EQ(50, down_right_skewport.y());
  EXPECT_EQ(120, down_right_skewport.width());
  EXPECT_EQ(175, down_right_skewport.height());
  EXPECT_TRUE(down_right_skewport.Contains(gfx::Rect(10, 50, 100, 100)));

  // Move viewport left.
  gfx::Rect left_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(-50, 0, 100, 100));

  EXPECT_EQ(-125, left_skewport.x());
  EXPECT_EQ(0, left_skewport.y());
  EXPECT_EQ(175, left_skewport.width());
  EXPECT_EQ(100, left_skewport.height());
  EXPECT_TRUE(left_skewport.Contains(gfx::Rect(-50, 0, 100, 100)));

  // Expand viewport.
  gfx::Rect expand_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(-50, -50, 200, 200));

  // x and y moved by -75 (-50 - 75 = -125).
  // right side and bottom side moved by 75 [(350 - 125) - (200 - 50) = 75].
  EXPECT_EQ(-125, expand_skewport.x());
  EXPECT_EQ(-125, expand_skewport.y());
  EXPECT_EQ(350, expand_skewport.width());
  EXPECT_EQ(350, expand_skewport.height());
  EXPECT_TRUE(expand_skewport.Contains(gfx::Rect(-50, -50, 200, 200)));

  // Expand the viewport past the limit.
  gfx::Rect big_expand_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(-500, -500, 1500, 1500));

  EXPECT_EQ(-575, big_expand_skewport.x());
  EXPECT_EQ(-575, big_expand_skewport.y());
  EXPECT_EQ(1650, big_expand_skewport.width());
  EXPECT_EQ(1650, big_expand_skewport.height());
  EXPECT_TRUE(big_expand_skewport.Contains(gfx::Rect(-500, -500, 1500, 1500)));
}

TEST(PictureLayerTilingTest, ComputeSkewport) {
  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Rect viewport(0, 0, 100, 100);
  gfx::Size layer_bounds(200, 200);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f, layer_bounds, &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 1.f, 1.0);

  // Move viewport down 50 pixels in 0.5 seconds.
  gfx::Rect down_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(0, 50, 100, 100));

  EXPECT_EQ(0, down_skewport.x());
  EXPECT_EQ(50, down_skewport.y());
  EXPECT_EQ(100, down_skewport.width());
  EXPECT_EQ(200, down_skewport.height());

  // Shrink viewport.
  gfx::Rect shrink_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(25, 25, 50, 50));

  EXPECT_EQ(25, shrink_skewport.x());
  EXPECT_EQ(25, shrink_skewport.y());
  EXPECT_EQ(50, shrink_skewport.width());
  EXPECT_EQ(50, shrink_skewport.height());

  // Move viewport down 50 and right 10 pixels.
  gfx::Rect down_right_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(10, 50, 100, 100));

  EXPECT_EQ(10, down_right_skewport.x());
  EXPECT_EQ(50, down_right_skewport.y());
  EXPECT_EQ(120, down_right_skewport.width());
  EXPECT_EQ(200, down_right_skewport.height());

  // Move viewport left.
  gfx::Rect left_skewport =
      tiling->ComputeSkewport(1.5, gfx::Rect(-20, 0, 100, 100));

  EXPECT_EQ(-60, left_skewport.x());
  EXPECT_EQ(0, left_skewport.y());
  EXPECT_EQ(140, left_skewport.width());
  EXPECT_EQ(100, left_skewport.height());

  // Expand viewport in 0.2 seconds.
  gfx::Rect expanded_skewport =
      tiling->ComputeSkewport(1.2, gfx::Rect(-5, -5, 110, 110));

  EXPECT_EQ(-30, expanded_skewport.x());
  EXPECT_EQ(-30, expanded_skewport.y());
  EXPECT_EQ(160, expanded_skewport.width());
  EXPECT_EQ(160, expanded_skewport.height());
}

TEST(PictureLayerTilingTest, ViewportDistanceWithScale) {
  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Rect viewport(0, 0, 100, 100);
  gfx::Size layer_bounds(1500, 1500);

  client.SetTileSize(gfx::Size(10, 10));

  // Tiling at 0.25 scale: this should create 47x47 tiles of size 10x10.
  // The reason is that each tile has a one pixel border, so tile at (1, 2)
  // for instance begins at (8, 16) pixels. So tile at (46, 46) will begin at
  // (368, 368) and extend to the end of 1500 * 0.25 = 375 edge of the
  // tiling.
  tiling = TestablePictureLayerTiling::Create(0.25f, layer_bounds, &client);
  gfx::Rect viewport_in_content_space =
      gfx::ToEnclosedRect(gfx::ScaleRect(viewport, 0.25f));

  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 1.f, 1.0);

  gfx::Rect soon_rect = viewport;
  soon_rect.Inset(-312.f, -312.f, -312.f, -312.f);
  gfx::Rect soon_rect_in_content_space =
      gfx::ToEnclosedRect(gfx::ScaleRect(soon_rect, 0.25f));

  // Sanity checks.
  for (int i = 0; i < 47; ++i) {
    for (int j = 0; j < 47; ++j) {
      EXPECT_TRUE(tiling->TileAt(i, j)) << "i: " << i << " j: " << j;
    }
  }
  for (int i = 0; i < 47; ++i) {
    EXPECT_FALSE(tiling->TileAt(i, 47)) << "i: " << i;
    EXPECT_FALSE(tiling->TileAt(47, i)) << "i: " << i;
  }

  // No movement in the viewport implies that tiles will either be NOW
  // or EVENTUALLY, with the exception of tiles that are between 0 and 312
  // pixels away from the viewport, which will be in the SOON bin.
  bool have_now = false;
  bool have_eventually = false;
  bool have_soon = false;
  for (int i = 0; i < 47; ++i) {
    for (int j = 0; j < 47; ++j) {
      Tile* tile = tiling->TileAt(i, j);
      TilePriority priority = tile->priority(ACTIVE_TREE);

      if (viewport_in_content_space.Intersects(tile->content_rect())) {
        EXPECT_EQ(TilePriority::NOW, priority.priority_bin);
        EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
        have_now = true;
      } else if (soon_rect_in_content_space.Intersects(tile->content_rect())) {
        EXPECT_EQ(TilePriority::SOON, priority.priority_bin);
        have_soon = true;
      } else {
        EXPECT_EQ(TilePriority::EVENTUALLY, priority.priority_bin);
        EXPECT_GT(priority.distance_to_visible, 0.f);
        have_eventually = true;
      }
    }
  }

  EXPECT_TRUE(have_now);
  EXPECT_TRUE(have_soon);
  EXPECT_TRUE(have_eventually);

  // Spot check some distances.
  // Tile at 5, 1 should begin at 41x9 in content space (without borders),
  // so the distance to a viewport that ends at 25x25 in content space
  // should be 17 (41 - 25 + 1). In layer space, then that should be
  // 17 / 0.25 = 68 pixels.

  // We can verify that the content rect (with borders) is one pixel off
  // 41,9 8x8 on all sides.
  EXPECT_EQ(tiling->TileAt(5, 1)->content_rect().ToString(), "40,8 10x10");

  TilePriority priority = tiling->TileAt(5, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(68.f, priority.distance_to_visible);

  priority = tiling->TileAt(2, 5)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(68.f, priority.distance_to_visible);

  priority = tiling->TileAt(3, 4)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(40.f, priority.distance_to_visible);

  // Move the viewport down 40 pixels.
  viewport = gfx::Rect(0, 40, 100, 100);
  viewport_in_content_space =
      gfx::ToEnclosedRect(gfx::ScaleRect(viewport, 0.25f));
  gfx::Rect skewport = tiling->ComputeSkewport(2.0, viewport_in_content_space);

  soon_rect = viewport;
  soon_rect.Inset(-312.f, -312.f, -312.f, -312.f);
  soon_rect_in_content_space =
      gfx::ToEnclosedRect(gfx::ScaleRect(soon_rect, 0.25f));

  EXPECT_EQ(0, skewport.x());
  EXPECT_EQ(10, skewport.y());
  EXPECT_EQ(25, skewport.width());
  EXPECT_EQ(35, skewport.height());

  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 1.f, 2.0);

  have_now = false;
  have_eventually = false;
  have_soon = false;

  // Viewport moved, so we expect to find some NOW tiles, some SOON tiles and
  // some EVENTUALLY tiles.
  for (int i = 0; i < 47; ++i) {
    for (int j = 0; j < 47; ++j) {
      Tile* tile = tiling->TileAt(i, j);
      TilePriority priority = tile->priority(ACTIVE_TREE);

      if (viewport_in_content_space.Intersects(tile->content_rect())) {
        EXPECT_EQ(TilePriority::NOW, priority.priority_bin) << "i: " << i
                                                            << " j: " << j;
        EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible) << "i: " << i
                                                           << " j: " << j;
        have_now = true;
      } else if (skewport.Intersects(tile->content_rect()) ||
                 soon_rect_in_content_space.Intersects(tile->content_rect())) {
        EXPECT_EQ(TilePriority::SOON, priority.priority_bin) << "i: " << i
                                                             << " j: " << j;
        EXPECT_GT(priority.distance_to_visible, 0.f) << "i: " << i
                                                     << " j: " << j;
        have_soon = true;
      } else {
        EXPECT_EQ(TilePriority::EVENTUALLY, priority.priority_bin)
            << "i: " << i << " j: " << j;
        EXPECT_GT(priority.distance_to_visible, 0.f) << "i: " << i
                                                     << " j: " << j;
        have_eventually = true;
      }
    }
  }

  EXPECT_TRUE(have_now);
  EXPECT_TRUE(have_soon);
  EXPECT_TRUE(have_eventually);

  priority = tiling->TileAt(5, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(68.f, priority.distance_to_visible);

  priority = tiling->TileAt(2, 5)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(28.f, priority.distance_to_visible);

  priority = tiling->TileAt(3, 4)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);

  // Change the underlying layer scale.
  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 2.0f, 3.0);

  priority = tiling->TileAt(5, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(34.f, priority.distance_to_visible);

  priority = tiling->TileAt(2, 5)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(14.f, priority.distance_to_visible);

  priority = tiling->TileAt(3, 4)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
}

TEST(PictureLayerTilingTest, ExpandRectEqual) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 10000);
  int64 target_area = 100 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(in.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectSmaller) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 10000);
  int64 target_area = 100 * 100;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.width() - in.width(), out.height() - in.height());
  EXPECT_NEAR(100 * 100, out.width() * out.height(), 50);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectUnbounded) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.width() - in.width(), out.height() - in.height());
  EXPECT_NEAR(200 * 200, out.width() * out.height(), 100);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedSmaller) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(50, 60, 40, 30);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedEqual) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds = in;
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedSmallerStretchVertical) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(45, 0, 90, 300);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedEqualStretchVertical) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(40, 0, 100, 300);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedSmallerStretchHorizontal) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(0, 55, 180, 190);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedEqualStretchHorizontal) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(0, 50, 180, 200);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectBoundedLeft) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(20, -1000, 10000, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.bottom() - in.bottom(), out.right() - in.right());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() - out.height() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedRight) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 1000+120, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_EQ(out.bottom() - in.bottom(), in.x() - out.x());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() - out.height() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedTop) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, 30, 10000, 10000);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.right() - in.right(), out.bottom() - in.bottom());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() * 2 - out.height());
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectBoundedBottom) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-1000, -1000, 10000, 1000 + 220);
  int64 target_area = 200 * 200;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_EQ(out.right() - in.right(), in.y() - out.y());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() * 2 - out.height());
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectSquishedHorizontally) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(0, -4000, 100+40+20, 100000);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(20, out.right() - in.right());
  EXPECT_EQ(40, in.x() - out.x());
  EXPECT_EQ(out.bottom() - in.bottom(), in.y() - out.y());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectSquishedVertically) {
  gfx::Rect in(40, 50, 100, 200);
  gfx::Rect bounds(-4000, 0, 100000, 200+50+30);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(30, out.bottom() - in.bottom());
  EXPECT_EQ(50, in.y() - out.y());
  EXPECT_EQ(out.right() - in.right(), in.x() - out.x());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.height() * 2);
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, ExpandRectOutOfBoundsFarAway) {
  gfx::Rect in(400, 500, 100, 200);
  gfx::Rect bounds(0, 0, 10, 10);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_TRUE(out.IsEmpty());
}

TEST(PictureLayerTilingTest, ExpandRectOutOfBoundsExpandedFullyCover) {
  gfx::Rect in(40, 50, 100, 100);
  gfx::Rect bounds(0, 0, 10, 10);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.ToString(), out.ToString());
}

TEST(PictureLayerTilingTest, ExpandRectOutOfBoundsExpandedPartlyCover) {
  gfx::Rect in(600, 600, 100, 100);
  gfx::Rect bounds(0, 0, 500, 500);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_EQ(bounds.right(), out.right());
  EXPECT_EQ(bounds.bottom(), out.bottom());
  EXPECT_LE(out.width() * out.height(), target_area);
  EXPECT_GT(out.width() * out.height(),
            target_area - out.width() - out.height());
  EXPECT_TRUE(bounds.Contains(out));
}

TEST(PictureLayerTilingTest, EmptyStartingRect) {
  // If a layer has a non-invertible transform, then the starting rect
  // for the layer would be empty.
  gfx::Rect in(40, 40, 0, 0);
  gfx::Rect bounds(0, 0, 10, 10);
  int64 target_area = 400 * 400;
  gfx::Rect out = PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
      in, target_area, bounds, NULL);
  EXPECT_TRUE(out.IsEmpty());
}

TEST(PictureLayerTilingTest, TilingRasterTileIteratorStaticViewport) {
  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Rect viewport(50, 50, 100, 100);
  gfx::Size layer_bounds(800, 800);

  gfx::Rect soon_rect = viewport;
  soon_rect.Inset(-312.f, -312.f, -312.f, -312.f);

  client.SetTileSize(gfx::Size(30, 30));

  tiling = TestablePictureLayerTiling::Create(1.0f, layer_bounds, &client);
  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 1.0f, 1.0);

  PictureLayerTiling::TilingRasterTileIterator empty_iterator;
  EXPECT_FALSE(empty_iterator);

  std::vector<Tile*> all_tiles = tiling->AllTilesForTesting();

  // Sanity check.
  EXPECT_EQ(841u, all_tiles.size());

  // The explanation of each iteration is as follows:
  // 1. First iteration tests that we can get all of the tiles correctly.
  // 2. Second iteration ensures that we can get all of the tiles again (first
  //    iteration didn't change any tiles), as well set all tiles to be ready to
  //    draw.
  // 3. Third iteration ensures that no tiles are returned, since they were all
  //    marked as ready to draw.
  for (int i = 0; i < 3; ++i) {
    PictureLayerTiling::TilingRasterTileIterator it(tiling.get(), ACTIVE_TREE);

    // There are 3 bins in TilePriority.
    bool have_tiles[3] = {};

    // On the third iteration, we should get no tiles since everything was
    // marked as ready to draw.
    if (i == 2) {
      EXPECT_FALSE(it);
      continue;
    }

    EXPECT_TRUE(it);
    std::set<Tile*> unique_tiles;
    unique_tiles.insert(*it);
    Tile* last_tile = *it;
    have_tiles[last_tile->priority(ACTIVE_TREE).priority_bin] = true;

    // On the second iteration, mark everything as ready to draw (solid color).
    if (i == 1) {
      ManagedTileState::TileVersion& tile_version =
          last_tile->GetTileVersionForTesting(
              last_tile->DetermineRasterModeForTree(ACTIVE_TREE));
      tile_version.SetSolidColorForTesting(SK_ColorRED);
    }
    ++it;
    int eventually_bin_order_correct_count = 0;
    int eventually_bin_order_incorrect_count = 0;
    while (it) {
      Tile* new_tile = *it;
      ++it;
      unique_tiles.insert(new_tile);

      TilePriority last_priority = last_tile->priority(ACTIVE_TREE);
      TilePriority new_priority = new_tile->priority(ACTIVE_TREE);
      EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
      if (last_priority.priority_bin == new_priority.priority_bin) {
        if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
          bool order_correct = last_priority.distance_to_visible <=
                               new_priority.distance_to_visible;
          eventually_bin_order_correct_count += order_correct;
          eventually_bin_order_incorrect_count += !order_correct;
        } else if (!soon_rect.Intersects(new_tile->content_rect()) &&
                   !soon_rect.Intersects(last_tile->content_rect())) {
          EXPECT_LE(last_priority.distance_to_visible,
                    new_priority.distance_to_visible);
          EXPECT_EQ(TilePriority::NOW, new_priority.priority_bin);
        } else if (new_priority.distance_to_visible > 0.f) {
          EXPECT_EQ(TilePriority::SOON, new_priority.priority_bin);
        }
      }
      have_tiles[new_priority.priority_bin] = true;

      last_tile = new_tile;

      // On the second iteration, mark everything as ready to draw (solid
      // color).
      if (i == 1) {
        ManagedTileState::TileVersion& tile_version =
            last_tile->GetTileVersionForTesting(
                last_tile->DetermineRasterModeForTree(ACTIVE_TREE));
        tile_version.SetSolidColorForTesting(SK_ColorRED);
      }
    }

    EXPECT_GT(eventually_bin_order_correct_count,
              eventually_bin_order_incorrect_count);

    // We should have now and eventually tiles, as well as soon tiles from
    // the border region.
    EXPECT_TRUE(have_tiles[TilePriority::NOW]);
    EXPECT_TRUE(have_tiles[TilePriority::SOON]);
    EXPECT_TRUE(have_tiles[TilePriority::EVENTUALLY]);

    EXPECT_EQ(unique_tiles.size(), all_tiles.size());
  }
}

TEST(PictureLayerTilingTest, TilingRasterTileIteratorMovingViewport) {
  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Rect viewport(50, 0, 100, 100);
  gfx::Rect moved_viewport(50, 0, 100, 500);
  gfx::Size layer_bounds(1000, 1000);

  client.SetTileSize(gfx::Size(30, 30));

  tiling = TestablePictureLayerTiling::Create(1.f, layer_bounds, &client);
  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 1.0f, 1.0);
  tiling->UpdateTilePriorities(ACTIVE_TREE, moved_viewport, 1.0f, 2.0);

  gfx::Rect soon_rect = moved_viewport;
  soon_rect.Inset(-312.f, -312.f, -312.f, -312.f);

  // There are 3 bins in TilePriority.
  bool have_tiles[3] = {};
  Tile* last_tile = NULL;
  int eventually_bin_order_correct_count = 0;
  int eventually_bin_order_incorrect_count = 0;
  for (PictureLayerTiling::TilingRasterTileIterator it(tiling.get(),
                                                       ACTIVE_TREE);
       it;
       ++it) {
    if (!last_tile)
      last_tile = *it;

    Tile* new_tile = *it;

    TilePriority last_priority = last_tile->priority(ACTIVE_TREE);
    TilePriority new_priority = new_tile->priority(ACTIVE_TREE);

    have_tiles[new_priority.priority_bin] = true;

    EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
    if (last_priority.priority_bin == new_priority.priority_bin) {
      if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
        bool order_correct = last_priority.distance_to_visible <=
                             new_priority.distance_to_visible;
        eventually_bin_order_correct_count += order_correct;
        eventually_bin_order_incorrect_count += !order_correct;
      } else if (!soon_rect.Intersects(new_tile->content_rect()) &&
                 !soon_rect.Intersects(last_tile->content_rect())) {
        EXPECT_LE(last_priority.distance_to_visible,
                  new_priority.distance_to_visible);
      } else if (new_priority.distance_to_visible > 0.f) {
        EXPECT_EQ(TilePriority::SOON, new_priority.priority_bin);
      }
    }
    last_tile = new_tile;
  }

  EXPECT_GT(eventually_bin_order_correct_count,
            eventually_bin_order_incorrect_count);

  EXPECT_TRUE(have_tiles[TilePriority::NOW]);
  EXPECT_TRUE(have_tiles[TilePriority::SOON]);
  EXPECT_TRUE(have_tiles[TilePriority::EVENTUALLY]);
}

static void TileExists(bool exists, Tile* tile,
                       const gfx::Rect& geometry_rect) {
  EXPECT_EQ(exists, tile != NULL) << geometry_rect.ToString();
}

TEST(PictureLayerTilingTest, TilingEvictionTileIteratorStaticViewport) {
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<FakeOutputSurface> output_surface = FakeOutputSurface::Create3d();
  CHECK(output_surface->BindToClient(&output_surface_client));
  TestSharedBitmapManager shared_bitmap_manager;
  scoped_ptr<ResourceProvider> resource_provider = ResourceProvider::Create(
      output_surface.get(), &shared_bitmap_manager, 0, false, 1, false);

  FakePictureLayerTilingClient client(resource_provider.get());
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Rect viewport(50, 50, 100, 100);
  gfx::Size layer_bounds(200, 200);

  client.SetTileSize(gfx::Size(30, 30));

  tiling = TestablePictureLayerTiling::Create(1.0f, layer_bounds, &client);
  tiling->UpdateTilePriorities(ACTIVE_TREE, viewport, 1.0f, 1.0);

  PictureLayerTiling::TilingRasterTileIterator empty_iterator;
  EXPECT_FALSE(empty_iterator);

  std::vector<Tile*> all_tiles = tiling->AllTilesForTesting();

  PictureLayerTiling::TilingEvictionTileIterator it(tiling.get(),
                                                    SMOOTHNESS_TAKES_PRIORITY);

  // Tiles don't have resources to evict.
  EXPECT_FALSE(it);

  // Sanity check.
  EXPECT_EQ(64u, all_tiles.size());

  client.tile_manager()->InitializeTilesWithResourcesForTesting(all_tiles);

  std::set<Tile*> all_tiles_set(all_tiles.begin(), all_tiles.end());

  it = PictureLayerTiling::TilingEvictionTileIterator(
      tiling.get(), SMOOTHNESS_TAKES_PRIORITY);
  EXPECT_TRUE(it);

  std::set<Tile*> eviction_tiles;
  Tile* last_tile = *it;
  for (; it; ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);
    EXPECT_LE(tile->priority(ACTIVE_TREE).priority_bin,
              last_tile->priority(ACTIVE_TREE).priority_bin);
    if (tile->priority(ACTIVE_TREE).priority_bin ==
        last_tile->priority(ACTIVE_TREE).priority_bin) {
      EXPECT_LE(tile->priority(ACTIVE_TREE).distance_to_visible,
                last_tile->priority(ACTIVE_TREE).distance_to_visible);
    }
    last_tile = tile;
    eviction_tiles.insert(tile);
  }

  EXPECT_GT(all_tiles_set.size(), 0u);
  EXPECT_EQ(all_tiles_set, eviction_tiles);
}

TEST_F(PictureLayerTilingIteratorTest, TilesExist) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      gfx::Rect(layer_bounds),  // visible content rect
      1.f,                      // current contents scale
      1.0);                     // current frame time
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));

  // Make the viewport rect empty. All tiles are killed and become zombies.
  tiling_->UpdateTilePriorities(ACTIVE_TREE,
                                gfx::Rect(),  // visible content rect
                                1.f,          // current contents scale
                                2.0);         // current frame time
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));
}

TEST_F(PictureLayerTilingIteratorTest, TilesExistGiantViewport) {
  gfx::Size layer_bounds(1099, 801);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  gfx::Rect giant_rect(-10000000, -10000000, 1000000000, 1000000000);

  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      gfx::Rect(layer_bounds),  // visible content rect
      1.f,                      // current contents scale
      1.0);                     // current frame time
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, true));

  // If the visible content rect is empty, it should still have live tiles.
  tiling_->UpdateTilePriorities(ACTIVE_TREE,
                                giant_rect,  // visible content rect
                                1.f,         // current contents scale
                                2.0);        // current frame time
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

  tiling_->UpdateTilePriorities(ACTIVE_TREE,
                                viewport_rect,  // visible content rect
                                1.f,            // current contents scale
                                1.0);           // current frame time
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
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  gfx::Rect visible_rect(8000, 8000, 50, 50);

  set_max_tiles_for_interest_area(1);
  tiling_->UpdateTilePriorities(ACTIVE_TREE,
                                visible_rect,  // visible content rect
                                1.f,           // current contents scale
                                1.0);          // current frame time
  VerifyTiles(1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TilesIntersectingRectExist, visible_rect, true));
}

static void CountExistingTiles(int *count,
                               Tile* tile,
                               const gfx::Rect& geometry_rect) {
  if (tile != NULL)
    ++(*count);
}

TEST_F(PictureLayerTilingIteratorTest,
       TilesExistLargeViewportAndLayerWithLargeVisibleArea) {
  gfx::Size layer_bounds(10000, 10000);
  Initialize(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds), base::Bind(&TileExists, false));

  set_max_tiles_for_interest_area(1);
  tiling_->UpdateTilePriorities(
      ACTIVE_TREE,
      gfx::Rect(layer_bounds),  // visible content rect
      1.f,                      // current contents scale
      1.0);                     // current frame time

  int num_tiles = 0;
  VerifyTiles(1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&CountExistingTiles, &num_tiles));
  // If we're making a rect the size of one tile, it can only overlap up to 4
  // tiles depending on its position.
  EXPECT_LE(num_tiles, 4);
  VerifyTiles(1.f, gfx::Rect(), base::Bind(&TileExists, false));
}

TEST_F(PictureLayerTilingIteratorTest, AddTilingsToMatchScale) {
  gfx::Size layer_bounds(1099, 801);
  gfx::Size tile_size(100, 100);

  client_.SetTileSize(tile_size);

  PictureLayerTilingSet active_set(&client_, layer_bounds);

  active_set.AddTiling(1.f);

  VerifyTiles(active_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, false));

  UpdateAllTilePriorities(&active_set,
                          PENDING_TREE,
                          gfx::Rect(layer_bounds),  // visible content rect
                          1.f,                      // current contents scale
                          1.0);                     // current frame time

  // The active tiling has tiles now.
  VerifyTiles(active_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));

  // Add the same tilings to the pending set.
  PictureLayerTilingSet pending_set(&client_, layer_bounds);
  Region invalidation;
  pending_set.SyncTilings(active_set, layer_bounds, invalidation, 0.f);

  // The pending tiling starts with no tiles.
  VerifyTiles(pending_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, false));

  // UpdateTilePriorities on the pending tiling at the same frame time. The
  // pending tiling should get tiles.
  UpdateAllTilePriorities(&pending_set,
                          PENDING_TREE,
                          gfx::Rect(layer_bounds),  // visible content rect
                          1.f,                      // current contents scale
                          1.0);                     // current frame time

  VerifyTiles(pending_set.tiling_at(0),
              1.f,
              gfx::Rect(layer_bounds),
              base::Bind(&TileExists, true));
}

TEST(UpdateTilePrioritiesTest, VisibleTiles) {
  // The TilePriority of visible tiles should have zero distance_to_visible
  // and time_to_visible.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);
}

TEST(UpdateTilePrioritiesTest, OffscreenTiles) {
  // The TilePriority of offscreen tiles (without movement) should have nonzero
  // distance_to_visible and infinite time_to_visible.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;

  current_screen_transform.Translate(850, 0);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  // Furthermore, in this scenario tiles on the right hand side should have a
  // larger distance to visible.
  TilePriority left = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  TilePriority right = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(right.distance_to_visible, left.distance_to_visible);

  left = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  right = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(right.distance_to_visible, left.distance_to_visible);
}

TEST(UpdateTilePrioritiesTest, PartiallyOffscreenLayer) {
  // Sanity check that a layer with some tiles visible and others offscreen has
  // correct TilePriorities for each tile.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;

  current_screen_transform.Translate(705, 505);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);
}

TEST(UpdateTilePrioritiesTest, PartiallyOffscreenRotatedLayer) {
  // Each tile of a layer may be affected differently by a transform; Check
  // that UpdateTilePriorities correctly accounts for the transform between
  // layer space and screen space.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;

  // A diagonally rotated layer that is partially off the bottom of the screen.
  // In this configuration, only the top-left tile would be visible.
  current_screen_transform.Translate(600, 750);
  current_screen_transform.RotateAboutZAxis(45);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  // Furthermore, in this scenario the bottom-right tile should have the larger
  // distance to visible.
  TilePriority top_left = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  TilePriority top_right = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  TilePriority bottom_right = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(top_right.distance_to_visible, top_left.distance_to_visible);

  EXPECT_EQ(bottom_right.distance_to_visible, top_right.distance_to_visible);
}

TEST(UpdateTilePrioritiesTest, PerspectiveLayer) {
  // Perspective transforms need to take a different code path.
  // This test checks tile priorities of a perspective layer.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);  // offscreen.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;

  // A 3d perspective layer rotated about its Y axis, translated to almost
  // fully offscreen. The left side will appear closer (i.e. larger in 2d) than
  // the right side, so the top-left tile will technically be closer than the
  // top-right.

  // Translate layer to offscreen
  current_screen_transform.Translate(400.0, 630.0);
  // Apply perspective about the center of the layer
  current_screen_transform.Translate(100.0, 100.0);
  current_screen_transform.ApplyPerspectiveDepth(100.0);
  current_screen_transform.RotateAboutYAxis(10.0);
  current_screen_transform.Translate(-100.0, -100.0);
  last_screen_transform = current_screen_transform;

  // Sanity check that this transform wouldn't cause w<0 clipping.
  bool clipped;
  MathUtil::MapQuad(current_screen_transform,
                    gfx::QuadF(gfx::RectF(0, 0, 200, 200)),
                    &clipped);
  ASSERT_FALSE(clipped);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  // All tiles will have a positive distance_to_visible
  // and an infinite time_to_visible.
  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(priority.distance_to_visible, 0.f);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(priority.distance_to_visible, 0.f);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  // Furthermore, in this scenario the top-left distance_to_visible
  // will be smallest, followed by top-right. The bottom layers
  // will of course be further than the top layers.
  TilePriority top_left = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  TilePriority top_right = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  TilePriority bottom_left = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  TilePriority bottom_right = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);

  EXPECT_GT(bottom_right.distance_to_visible, top_right.distance_to_visible);

  EXPECT_GT(bottom_left.distance_to_visible, top_left.distance_to_visible);
}

TEST(UpdateTilePrioritiesTest, PerspectiveLayerClippedByW) {
  // Perspective transforms need to take a different code path.
  // This test checks tile priorities of a perspective layer.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double current_frame_time_in_seconds = 1.0;

  // A 3d perspective layer rotated about its Y axis, translated to almost
  // fully offscreen. The left side will appear closer (i.e. larger in 2d) than
  // the right side, so the top-left tile will technically be closer than the
  // top-right.

  // Translate layer to offscreen
  current_screen_transform.Translate(400.0, 970.0);
  // Apply perspective and rotation about the center of the layer
  current_screen_transform.Translate(100.0, 100.0);
  current_screen_transform.ApplyPerspectiveDepth(10.0);
  current_screen_transform.RotateAboutYAxis(10.0);
  current_screen_transform.Translate(-100.0, -100.0);
  last_screen_transform = current_screen_transform;

  // Sanity check that this transform does cause w<0 clipping for the left side
  // of the layer, but not the right side.
  bool clipped;
  MathUtil::MapQuad(current_screen_transform,
                    gfx::QuadF(gfx::RectF(0, 0, 100, 200)),
                    &clipped);
  ASSERT_TRUE(clipped);

  MathUtil::MapQuad(current_screen_transform,
                    gfx::QuadF(gfx::RectF(100, 0, 100, 200)),
                    &clipped);
  ASSERT_FALSE(clipped);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  // Left-side tiles will be clipped by the transform, so we have to assume
  // they are visible just in case.
  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  // Right-side tiles will have a positive distance_to_visible
  // and an infinite time_to_visible.
  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(priority.distance_to_visible, 0.f);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);
}

TEST(UpdateTilePrioritiesTest, BasicMotion) {
  // Test that time_to_visible is computed correctly when
  // there is some motion.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double last_frame_time_in_seconds = 1.0;
  double current_frame_time_in_seconds = 2.0;

  // Offscreen layer is coming closer to viewport at 1000 pixels per second.
  current_screen_transform.Translate(1800, 0);
  last_screen_transform.Translate(2800, 0);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  // previous ("last") frame
  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               last_layer_contents_scale,
                               last_frame_time_in_seconds);

  // current frame
  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  // time_to_visible for the right hand side layers needs an extra 0.099
  // seconds because this tile is 99 pixels further away.
  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 1)->priority(ACTIVE_TREE);
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);
}

TEST(UpdateTilePrioritiesTest, RotationMotion) {
  // Each tile of a layer may be affected differently by a transform; Check
  // that UpdateTilePriorities correctly accounts for the transform between
  // layer space and screen space.

  FakePictureLayerTilingClient client;
  scoped_ptr<TestablePictureLayerTiling> tiling;

  gfx::Size device_viewport(800, 600);
  gfx::Rect visible_layer_rect(0, 0, 0, 0);  // offscren.
  gfx::Size last_layer_bounds(200, 200);
  gfx::Size current_layer_bounds(200, 200);
  float last_layer_contents_scale = 1.f;
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;
  double last_frame_time_in_seconds = 1.0;
  double current_frame_time_in_seconds = 2.0;

  // Rotation motion is set up specifically so that:
  //  - rotation occurs about the center of the layer
  //  - the top-left tile becomes visible on rotation
  //  - the top-right tile will have an infinite time_to_visible
  //    because it is rotating away from viewport.
  //  - bottom-left layer will have a positive non-zero time_to_visible
  //    because it is rotating toward the viewport.
  current_screen_transform.Translate(400, 550);
  current_screen_transform.RotateAboutZAxis(45);

  last_screen_transform.Translate(400, 550);

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));
  tiling = TestablePictureLayerTiling::Create(1.0f,  // contents_scale
                                              current_layer_bounds,
                                              &client);

  // previous ("last") frame
  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               last_layer_contents_scale,
                               last_frame_time_in_seconds);

  // current frame
  tiling->UpdateTilePriorities(ACTIVE_TREE,
                               viewport_in_layer_space,
                               current_layer_contents_scale,
                               current_frame_time_in_seconds);

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = tiling->TileAt(0, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(0, 1)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = tiling->TileAt(1, 0)->priority(ACTIVE_TREE);
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_EQ(TilePriority::NOW, priority.priority_bin);
}

}  // namespace
}  // namespace cc
