// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "cc/resources/picture_pile.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

class TestPicturePile : public PicturePile {
 public:
  using PicturePile::buffer_pixels;
  using PicturePile::CanRasterSlowTileCheck;
  using PicturePile::Clear;

  PictureMap& picture_map() { return picture_map_; }
  const gfx::Rect& recorded_viewport() const { return recorded_viewport_; }

  bool CanRasterLayerRect(const gfx::Rect& layer_rect) {
    return CanRaster(1.f, layer_rect);
  }

  typedef PicturePile::PictureInfo PictureInfo;
  typedef PicturePile::PictureMapKey PictureMapKey;
  typedef PicturePile::PictureMap PictureMap;

 protected:
    virtual ~TestPicturePile() {}
};

class PicturePileTest : public testing::Test {
 public:
  PicturePileTest()
      : pile_(new TestPicturePile()),
        background_color_(SK_ColorBLUE),
        min_scale_(0.125),
        frame_number_(0),
        contents_opaque_(false) {
    pile_->SetTilingRect(gfx::Rect(pile_->tiling().max_texture_size()));
    pile_->SetTileGridSize(gfx::Size(1000, 1000));
    pile_->SetMinContentsScale(min_scale_);
  }

  gfx::Rect tiling_rect() const { return pile_->tiling_rect(); }

  bool UpdateAndExpandInvalidation(Region* invalidation,
                                   const gfx::Rect& visible_layer_rect) {
    frame_number_++;
    return pile_->UpdateAndExpandInvalidation(&client_,
                                              invalidation,
                                              background_color_,
                                              contents_opaque_,
                                              false,
                                              visible_layer_rect,
                                              frame_number_,
                                              Picture::RECORD_NORMALLY,
                                              &stats_instrumentation_);
  }

  bool UpdateWholePile() {
    Region invalidation = tiling_rect();
    bool result = UpdateAndExpandInvalidation(&invalidation, tiling_rect());
    EXPECT_EQ(tiling_rect().ToString(), invalidation.ToString());
    return result;
  }

  FakeContentLayerClient client_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  scoped_refptr<TestPicturePile> pile_;
  SkColor background_color_;
  float min_scale_;
  int frame_number_;
  bool contents_opaque_;
};

TEST_F(PicturePileTest, SmallInvalidateInflated) {
  UpdateWholePile();

  // Invalidate something inside a tile.
  Region invalidate_rect(gfx::Rect(50, 50, 1, 1));
  UpdateAndExpandInvalidation(&invalidate_rect, tiling_rect());
  EXPECT_EQ(gfx::Rect(50, 50, 1, 1).ToString(), invalidate_rect.ToString());

  EXPECT_EQ(1, pile_->tiling().num_tiles_x());
  EXPECT_EQ(1, pile_->tiling().num_tiles_y());

  TestPicturePile::PictureInfo& picture_info =
      pile_->picture_map().find(TestPicturePile::PictureMapKey(0, 0))->second;
  // We should have a picture.
  EXPECT_TRUE(!!picture_info.GetPicture());
  gfx::Rect picture_rect = gfx::ScaleToEnclosedRect(
      picture_info.GetPicture()->LayerRect(), min_scale_);

  // The the picture should be large enough that scaling it never makes a rect
  // smaller than 1 px wide or tall.
  EXPECT_FALSE(picture_rect.IsEmpty()) << "Picture rect " <<
      picture_rect.ToString();
}

TEST_F(PicturePileTest, LargeInvalidateInflated) {
  UpdateWholePile();

  // Invalidate something inside a tile.
  Region invalidate_rect(gfx::Rect(50, 50, 100, 100));
  UpdateAndExpandInvalidation(&invalidate_rect, tiling_rect());
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100).ToString(), invalidate_rect.ToString());

  EXPECT_EQ(1, pile_->tiling().num_tiles_x());
  EXPECT_EQ(1, pile_->tiling().num_tiles_y());

  TestPicturePile::PictureInfo& picture_info =
      pile_->picture_map().find(TestPicturePile::PictureMapKey(0, 0))->second;
  EXPECT_TRUE(!!picture_info.GetPicture());

  int expected_inflation = pile_->buffer_pixels();

  Picture* base_picture = picture_info.GetPicture();
  gfx::Rect base_picture_rect = pile_->tiling_rect();
  base_picture_rect.Inset(-expected_inflation, -expected_inflation);
  EXPECT_EQ(base_picture_rect.ToString(),
            base_picture->LayerRect().ToString());
}

TEST_F(PicturePileTest, InvalidateOnTileBoundaryInflated) {
  gfx::Rect new_tiling_rect =
      gfx::ToEnclosedRect(gfx::ScaleRect(pile_->tiling_rect(), 2.f));
  pile_->SetTilingRect(new_tiling_rect);

  // Due to border pixels, we should have 3 tiles.
  EXPECT_EQ(3, pile_->tiling().num_tiles_x());
  EXPECT_EQ(3, pile_->tiling().num_tiles_y());

  // We should have 1/.125 - 1 = 7 border pixels.
  EXPECT_EQ(7, pile_->buffer_pixels());
  EXPECT_EQ(7, pile_->tiling().border_texels());

  // Update the whole layer to create initial pictures.
  UpdateWholePile();

  // Invalidate everything again to have a non zero invalidation
  // frequency.
  UpdateWholePile();

  // Invalidate something just over a tile boundary by a single pixel.
  // This will invalidate the tile (1, 1), as well as 1 row of pixels in (1, 0).
  Region invalidate_rect(
      gfx::Rect(pile_->tiling().TileBoundsWithBorder(0, 0).right(),
                pile_->tiling().TileBoundsWithBorder(0, 0).bottom() - 1,
                50,
                50));
  Region expected_invalidation = invalidate_rect;
  UpdateAndExpandInvalidation(&invalidate_rect, tiling_rect());
  EXPECT_EQ(expected_invalidation.ToString(), invalidate_rect.ToString());

  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;

      // Expect (1, 1) and (1, 0) to be invalidated once more
      // than the rest of the tiles.
      if (i == 1 && (j == 0 || j == 1)) {
        EXPECT_FLOAT_EQ(
            2.0f / TestPicturePile::PictureInfo::INVALIDATION_FRAMES_TRACKED,
            picture_info.GetInvalidationFrequencyForTesting());
      } else {
        EXPECT_FLOAT_EQ(
            1.0f / TestPicturePile::PictureInfo::INVALIDATION_FRAMES_TRACKED,
            picture_info.GetInvalidationFrequencyForTesting());
      }
    }
  }
}

TEST_F(PicturePileTest, StopRecordingOffscreenInvalidations) {
  gfx::Rect new_tiling_rect =
      gfx::ToEnclosedRect(gfx::ScaleRect(pile_->tiling_rect(), 4.f));
  pile_->SetTilingRect(new_tiling_rect);

  gfx::Rect viewport(
      tiling_rect().x(), tiling_rect().y(), tiling_rect().width(), 1);

  // Update the whole pile until the invalidation frequency is high.
  for (int frame = 0; frame < 33; ++frame) {
    UpdateWholePile();
  }

  // Make sure we have a high invalidation frequency.
  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;
      EXPECT_FLOAT_EQ(1.0f, picture_info.GetInvalidationFrequencyForTesting())
          << "i " << i << " j " << j;
    }
  }

  // Update once more with a small viewport.
  Region invalidation = tiling_rect();
  UpdateAndExpandInvalidation(&invalidation, viewport);
  EXPECT_EQ(tiling_rect().ToString(), invalidation.ToString());

  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;
      EXPECT_FLOAT_EQ(1.0f, picture_info.GetInvalidationFrequencyForTesting());

      // If the y far enough away we expect to find no picture (no re-recording
      // happened). For close y, the picture should change.
      if (j >= 2)
        EXPECT_FALSE(picture_info.GetPicture()) << "i " << i << " j " << j;
      else
        EXPECT_TRUE(picture_info.GetPicture()) << "i " << i << " j " << j;
    }
  }

  // Update a partial tile that doesn't get recorded. We should expand the
  // invalidation to the entire tiles that overlap it.
  Region small_invalidation =
      gfx::Rect(pile_->tiling().TileBounds(3, 4).x(),
                pile_->tiling().TileBounds(3, 4).y() + 10,
                1,
                1);
  UpdateAndExpandInvalidation(&small_invalidation, viewport);
  EXPECT_TRUE(small_invalidation.Contains(gfx::UnionRects(
      pile_->tiling().TileBounds(2, 4), pile_->tiling().TileBounds(3, 4))))
      << small_invalidation.ToString();

  // Now update with no invalidation and full viewport
  Region empty_invalidation;
  UpdateAndExpandInvalidation(&empty_invalidation, tiling_rect());
  EXPECT_EQ(Region().ToString(), empty_invalidation.ToString());

  for (int i = 0; i < pile_->tiling().num_tiles_x(); ++i) {
    for (int j = 0; j < pile_->tiling().num_tiles_y(); ++j) {
      TestPicturePile::PictureInfo& picture_info =
          pile_->picture_map()
              .find(TestPicturePile::PictureMapKey(i, j))
              ->second;
      // Expect the invalidation frequency to be less than 1, since we just
      // updated with no invalidations.
      EXPECT_LT(picture_info.GetInvalidationFrequencyForTesting(), 1.f);

      // We expect that there are pictures everywhere now.
      EXPECT_TRUE(picture_info.GetPicture()) << "i " << i << " j " << j;
    }
  }
}

TEST_F(PicturePileTest, ClearingInvalidatesRecordedRect) {
  UpdateWholePile();

  gfx::Rect rect(0, 0, 5, 5);
  EXPECT_TRUE(pile_->CanRasterLayerRect(rect));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(rect));

  pile_->Clear();

  // Make sure both the cache-aware check (using recorded region) and the normal
  // check are both false after clearing.
  EXPECT_FALSE(pile_->CanRasterLayerRect(rect));
  EXPECT_FALSE(pile_->CanRasterSlowTileCheck(rect));
}

TEST_F(PicturePileTest, FrequentInvalidationCanRaster) {
  // This test makes sure that if part of the page is frequently invalidated
  // and doesn't get re-recorded, then CanRaster is not true for any
  // tiles touching it, but is true for adjacent tiles, even if it
  // overlaps on borders (edge case).
  gfx::Rect new_tiling_rect =
      gfx::ToEnclosedRect(gfx::ScaleRect(pile_->tiling_rect(), 4.f));
  pile_->SetTilingRect(new_tiling_rect);

  gfx::Rect tile01_borders = pile_->tiling().TileBoundsWithBorder(0, 1);
  gfx::Rect tile02_borders = pile_->tiling().TileBoundsWithBorder(0, 2);
  gfx::Rect tile01_noborders = pile_->tiling().TileBounds(0, 1);
  gfx::Rect tile02_noborders = pile_->tiling().TileBounds(0, 2);

  // Sanity check these two tiles are overlapping with borders, since this is
  // what the test is trying to repro.
  EXPECT_TRUE(tile01_borders.Intersects(tile02_borders));
  EXPECT_FALSE(tile01_noborders.Intersects(tile02_noborders));
  UpdateWholePile();
  EXPECT_TRUE(pile_->CanRasterLayerRect(tile01_noborders));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(tile01_noborders));
  EXPECT_TRUE(pile_->CanRasterLayerRect(tile02_noborders));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(tile02_noborders));
  // Sanity check that an initial paint goes down the fast path of having
  // a valid recorded viewport.
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());

  // Update the whole layer until the invalidation frequency is high.
  for (int frame = 0; frame < 33; ++frame) {
    UpdateWholePile();
  }

  // Update once more with a small viewport.
  gfx::Rect viewport(0, 0, tiling_rect().width(), 1);
  Region invalidation(tiling_rect());
  UpdateAndExpandInvalidation(&invalidation, viewport);
  EXPECT_EQ(tiling_rect().ToString(), invalidation.ToString());

  // Sanity check some pictures exist and others don't.
  EXPECT_TRUE(pile_->picture_map()
                  .find(TestPicturePile::PictureMapKey(0, 1))
                  ->second.GetPicture());
  EXPECT_FALSE(pile_->picture_map()
                   .find(TestPicturePile::PictureMapKey(0, 2))
                   ->second.GetPicture());

  EXPECT_TRUE(pile_->CanRasterLayerRect(tile01_noborders));
  EXPECT_TRUE(pile_->CanRasterSlowTileCheck(tile01_noborders));
  EXPECT_FALSE(pile_->CanRasterLayerRect(tile02_noborders));
  EXPECT_FALSE(pile_->CanRasterSlowTileCheck(tile02_noborders));
}

TEST_F(PicturePileTest, NoInvalidationValidViewport) {
  // This test validates that the recorded_viewport cache of full tiles
  // is still valid for some use cases.  If it's not, it's a performance
  // issue because CanRaster checks will go down the slow path.
  UpdateWholePile();
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());

  // No invalidation, same viewport.
  Region invalidation;
  UpdateAndExpandInvalidation(&invalidation, tiling_rect());
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());
  EXPECT_EQ(Region().ToString(), invalidation.ToString());

  // Partial invalidation, same viewport.
  invalidation = gfx::Rect(0, 0, 1, 1);
  UpdateAndExpandInvalidation(&invalidation, tiling_rect());
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());
  EXPECT_EQ(gfx::Rect(0, 0, 1, 1).ToString(), invalidation.ToString());

  // No invalidation, changing viewport.
  invalidation = Region();
  UpdateAndExpandInvalidation(&invalidation, gfx::Rect(5, 5, 5, 5));
  EXPECT_TRUE(!pile_->recorded_viewport().IsEmpty());
  EXPECT_EQ(Region().ToString(), invalidation.ToString());
}

TEST_F(PicturePileTest, InvalidationOutsideRecordingRect) {
  gfx::Rect huge_layer_rect(10000000, 20000000);
  gfx::Rect viewport(300000, 400000, 5000, 6000);

  pile_->SetTilingRect(huge_layer_rect);

  // Invalidation inside the recording rect does not need to be expanded.
  Region invalidation = viewport;
  UpdateAndExpandInvalidation(&invalidation, viewport);
  EXPECT_EQ(viewport.ToString(), invalidation.ToString());

  // Invalidation outside the recording rect should expand to the tiles it
  // covers.
  gfx::Rect recorded_over_tiles =
      pile_->tiling().ExpandRectToTileBounds(pile_->recorded_viewport());
  gfx::Rect invalidation_outside(
      recorded_over_tiles.right(), recorded_over_tiles.y(), 30, 30);
  invalidation = invalidation_outside;
  UpdateAndExpandInvalidation(&invalidation, viewport);
  gfx::Rect expanded_recorded_viewport =
      pile_->tiling().ExpandRectToTileBounds(pile_->recorded_viewport());
  Region expected_invalidation =
      pile_->tiling().ExpandRectToTileBounds(invalidation_outside);
  EXPECT_EQ(expected_invalidation.ToString(), invalidation.ToString());
}

}  // namespace
}  // namespace cc
