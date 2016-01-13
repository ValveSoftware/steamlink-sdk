// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/tiling_data.h"

#include <algorithm>
#include <vector>

#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

int NumTiles(const gfx::Size& max_texture_size,
             const gfx::Rect& tiling_rect,
             bool has_border_texels) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  int num_tiles = tiling.num_tiles_x() * tiling.num_tiles_y();

  // Assert no overflow.
  EXPECT_GE(num_tiles, 0);
  if (num_tiles > 0)
    EXPECT_EQ(num_tiles / tiling.num_tiles_x(), tiling.num_tiles_y());

  return num_tiles;
}

int XIndex(const gfx::Size& max_texture_size,
           const gfx::Rect& tiling_rect,
           bool has_border_texels,
           int x_coord) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.TileXIndexFromSrcCoord(x_coord);
}

int YIndex(const gfx::Size& max_texture_size,
           const gfx::Rect& tiling_rect,
           bool has_border_texels,
           int y_coord) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.TileYIndexFromSrcCoord(y_coord);
}

int MinBorderXIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    bool has_border_texels,
                    int x_coord) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.FirstBorderTileXIndexFromSrcCoord(x_coord);
}

int MinBorderYIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    bool has_border_texels,
                    int y_coord) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.FirstBorderTileYIndexFromSrcCoord(y_coord);
}

int MaxBorderXIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    bool has_border_texels,
                    int x_coord) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.LastBorderTileXIndexFromSrcCoord(x_coord);
}

int MaxBorderYIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    bool has_border_texels,
                    int y_coord) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.LastBorderTileYIndexFromSrcCoord(y_coord);
}

int PosX(const gfx::Size& max_texture_size,
         const gfx::Rect& tiling_rect,
         bool has_border_texels,
         int x_index) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.TilePositionX(x_index);
}

int PosY(const gfx::Size& max_texture_size,
         const gfx::Rect& tiling_rect,
         bool has_border_texels,
         int y_index) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.TilePositionY(y_index);
}

int SizeX(const gfx::Size& max_texture_size,
          const gfx::Rect& tiling_rect,
          bool has_border_texels,
          int x_index) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.TileSizeX(x_index);
}

int SizeY(const gfx::Size& max_texture_size,
          const gfx::Rect& tiling_rect,
          bool has_border_texels,
          int y_index) {
  TilingData tiling(max_texture_size, tiling_rect, has_border_texels);
  return tiling.TileSizeY(y_index);
}

class TilingDataTest : public ::testing::TestWithParam<gfx::Point> {};

TEST_P(TilingDataTest, NumTiles_NoTiling) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(16, 16), gfx::Rect(origin, gfx::Size(16, 16)), false));
  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(16, 16), gfx::Rect(origin, gfx::Size(15, 15)), true));
  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(16, 16), gfx::Rect(origin, gfx::Size(16, 16)), true));
  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(16, 16), gfx::Rect(origin, gfx::Size(1, 16)), false));
  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(15, 15), gfx::Rect(origin, gfx::Size(15, 15)), true));
  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(32, 16), gfx::Rect(origin, gfx::Size(32, 16)), false));
  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(32, 16), gfx::Rect(origin, gfx::Size(32, 16)), true));
}

TEST_P(TilingDataTest, NumTiles_TilingNoBorders) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(0, 0)), false));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(4, 0)), false));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(0, 4)), false));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(4, 0)), false));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(0, 4)), false));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(1, 1)), false));

  EXPECT_EQ(
      1, NumTiles(gfx::Size(1, 1), gfx::Rect(origin, gfx::Size(1, 1)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(1, 1), gfx::Rect(origin, gfx::Size(1, 2)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(1, 1), gfx::Rect(origin, gfx::Size(2, 1)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(1, 1)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(1, 2)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(2, 1)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(2, 2)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 3)), false));

  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(1, 4)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(2, 4)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(3, 4)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(4, 4)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(5, 4)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(6, 4)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(7, 4)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(8, 4)), false));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(9, 4)), false));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(10, 4)), false));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(11, 4)), false));

  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(1, 5)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(2, 5)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(3, 5)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(4, 5)), false));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(5, 5)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 5)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(7, 5)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 5)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(9, 5)), false));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 5)), false));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(11, 5)), false));

  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(16, 16), gfx::Rect(origin, gfx::Size(16, 16)), false));
  EXPECT_EQ(
      1,
      NumTiles(gfx::Size(17, 17), gfx::Rect(origin, gfx::Size(16, 16)), false));
  EXPECT_EQ(
      4,
      NumTiles(gfx::Size(15, 15), gfx::Rect(origin, gfx::Size(16, 16)), false));
  EXPECT_EQ(
      4,
      NumTiles(gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(16, 16)), false));
  EXPECT_EQ(
      6,
      NumTiles(gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(17, 16)), false));

  EXPECT_EQ(
      8,
      NumTiles(gfx::Size(5, 8), gfx::Rect(origin, gfx::Size(17, 16)), false));
}

TEST_P(TilingDataTest, NumTiles_TilingWithBorders) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(0, 0)), true));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(4, 0)), true));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(0, 4)), true));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(4, 0)), true));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(0, 4)), true));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(1, 1)), true));

  EXPECT_EQ(
      1, NumTiles(gfx::Size(1, 1), gfx::Rect(origin, gfx::Size(1, 1)), true));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(1, 1), gfx::Rect(origin, gfx::Size(1, 2)), true));
  EXPECT_EQ(
      0, NumTiles(gfx::Size(1, 1), gfx::Rect(origin, gfx::Size(2, 1)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(1, 1)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(1, 2)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(2, 1)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(2, 2), gfx::Rect(origin, gfx::Size(2, 2)), true));

  EXPECT_EQ(
      1, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 3)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(2, 3)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 3)), true));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(4, 3)), true));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(5, 3)), true));
  EXPECT_EQ(
      4, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 3)), true));
  EXPECT_EQ(
      5, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(7, 3)), true));

  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(1, 4)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(2, 4)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(3, 4)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(4, 4)), true));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(5, 4)), true));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(6, 4)), true));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(7, 4)), true));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(8, 4)), true));
  EXPECT_EQ(
      4, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(9, 4)), true));
  EXPECT_EQ(
      4, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(10, 4)), true));
  EXPECT_EQ(
      5, NumTiles(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(11, 4)), true));

  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(1, 5)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(2, 5)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(3, 5)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(4, 5)), true));
  EXPECT_EQ(
      1, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(5, 5)), true));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 5)), true));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(7, 5)), true));
  EXPECT_EQ(
      2, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 5)), true));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(9, 5)), true));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 5)), true));
  EXPECT_EQ(
      3, NumTiles(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(11, 5)), true));

  EXPECT_EQ(
      30,
      NumTiles(gfx::Size(8, 5), gfx::Rect(origin, gfx::Size(16, 32)), true));
}

TEST_P(TilingDataTest, TileXIndexFromSrcCoord) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 1));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 2));
  EXPECT_EQ(1,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 3));
  EXPECT_EQ(1,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 4));
  EXPECT_EQ(1,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 5));
  EXPECT_EQ(2,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 6));
  EXPECT_EQ(2,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 7));
  EXPECT_EQ(2,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 8));
  EXPECT_EQ(3,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 9));
  EXPECT_EQ(3,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 10));
  EXPECT_EQ(3,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.x() + 11));

  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x()));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 1));
  EXPECT_EQ(1,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 2));
  EXPECT_EQ(2,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 3));
  EXPECT_EQ(3,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 4));
  EXPECT_EQ(4,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 5));
  EXPECT_EQ(5,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 6));
  EXPECT_EQ(6,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 7));
  EXPECT_EQ(7,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 8));
  EXPECT_EQ(7,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 9));
  EXPECT_EQ(7,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 10));
  EXPECT_EQ(7,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.x() + 11));

  EXPECT_EQ(0,
            XIndex(gfx::Size(1, 1),
                   gfx::Rect(origin, gfx::Size(1, 1)),
                   false,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   false,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   false,
                   origin.x() + 1));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   false,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   false,
                   origin.x() + 1));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   false,
                   origin.x() + 2));

  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   false,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   false,
                   origin.x() + 1));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   false,
                   origin.x() + 2));
  EXPECT_EQ(1,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   false,
                   origin.x() + 3));

  EXPECT_EQ(0,
            XIndex(gfx::Size(1, 1),
                   gfx::Rect(origin, gfx::Size(1, 1)),
                   true,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   true,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   true,
                   origin.x() + 1));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   true,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   true,
                   origin.x() + 1));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   true,
                   origin.x() + 2));

  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   true,
                   origin.x() + 0));
  EXPECT_EQ(0,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   true,
                   origin.x() + 1));
  EXPECT_EQ(1,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   true,
                   origin.x() + 2));
  EXPECT_EQ(1,
            XIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(4, 3)),
                   true,
                   origin.x() + 3));
}

TEST_P(TilingDataTest, FirstBorderTileXIndexFromSrcCoord) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 2));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 3));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 4));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 5));
  EXPECT_EQ(2,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 6));
  EXPECT_EQ(2,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 7));
  EXPECT_EQ(2,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 8));
  EXPECT_EQ(3,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 9));
  EXPECT_EQ(3,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 10));
  EXPECT_EQ(3,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 11));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 2));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 3));
  EXPECT_EQ(2,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 4));
  EXPECT_EQ(3,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 5));
  EXPECT_EQ(4,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 6));
  EXPECT_EQ(5,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 7));
  EXPECT_EQ(6,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 8));
  EXPECT_EQ(7,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 9));
  EXPECT_EQ(7,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 10));
  EXPECT_EQ(7,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 11));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.x() + 2));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 2));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 3));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.x() + 2));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 2));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 3));
}

TEST_P(TilingDataTest, LastBorderTileXIndexFromSrcCoord) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 2));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 3));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 4));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 5));
  EXPECT_EQ(2,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 6));
  EXPECT_EQ(2,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 7));
  EXPECT_EQ(2,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 8));
  EXPECT_EQ(3,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 9));
  EXPECT_EQ(3,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 10));
  EXPECT_EQ(3,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.x() + 11));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(2,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 2));
  EXPECT_EQ(3,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 3));
  EXPECT_EQ(4,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 4));
  EXPECT_EQ(5,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 5));
  EXPECT_EQ(6,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 6));
  EXPECT_EQ(7,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 7));
  EXPECT_EQ(7,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 8));
  EXPECT_EQ(7,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 9));
  EXPECT_EQ(7,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 10));
  EXPECT_EQ(7,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.x() + 11));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.x() + 2));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 2));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            false,
                            origin.x() + 3));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.x() + 2));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 0));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 1));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 2));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(4, 3)),
                            true,
                            origin.x() + 3));
}

TEST_P(TilingDataTest, TileYIndexFromSrcCoord) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 1));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 2));
  EXPECT_EQ(1,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 3));
  EXPECT_EQ(1,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 4));
  EXPECT_EQ(1,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 5));
  EXPECT_EQ(2,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 6));
  EXPECT_EQ(2,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 7));
  EXPECT_EQ(2,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 8));
  EXPECT_EQ(3,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 9));
  EXPECT_EQ(3,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 10));
  EXPECT_EQ(3,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   false,
                   origin.y() + 11));

  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 1));
  EXPECT_EQ(1,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 2));
  EXPECT_EQ(2,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 3));
  EXPECT_EQ(3,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 4));
  EXPECT_EQ(4,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 5));
  EXPECT_EQ(5,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 6));
  EXPECT_EQ(6,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 7));
  EXPECT_EQ(7,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 8));
  EXPECT_EQ(7,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 9));
  EXPECT_EQ(7,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 10));
  EXPECT_EQ(7,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(10, 10)),
                   true,
                   origin.y() + 11));

  EXPECT_EQ(0,
            YIndex(gfx::Size(1, 1),
                   gfx::Rect(origin, gfx::Size(1, 1)),
                   false,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   false,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   false,
                   origin.y() + 1));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   false,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   false,
                   origin.y() + 1));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   false,
                   origin.y() + 2));

  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   false,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   false,
                   origin.y() + 1));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   false,
                   origin.y() + 2));
  EXPECT_EQ(1,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   false,
                   origin.y() + 3));

  EXPECT_EQ(0,
            YIndex(gfx::Size(1, 1),
                   gfx::Rect(origin, gfx::Size(1, 1)),
                   true,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   true,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(2, 2),
                   gfx::Rect(origin, gfx::Size(2, 2)),
                   true,
                   origin.y() + 1));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   true,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   true,
                   origin.y() + 1));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 3)),
                   true,
                   origin.y() + 2));

  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   true,
                   origin.y() + 0));
  EXPECT_EQ(0,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   true,
                   origin.y() + 1));
  EXPECT_EQ(1,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   true,
                   origin.y() + 2));
  EXPECT_EQ(1,
            YIndex(gfx::Size(3, 3),
                   gfx::Rect(origin, gfx::Size(3, 4)),
                   true,
                   origin.y() + 3));
}

TEST_P(TilingDataTest, FirstBorderTileYIndexFromSrcCoord) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 2));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 3));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 4));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 5));
  EXPECT_EQ(2,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 6));
  EXPECT_EQ(2,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 7));
  EXPECT_EQ(2,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 8));
  EXPECT_EQ(3,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 9));
  EXPECT_EQ(3,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 10));
  EXPECT_EQ(3,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 11));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 2));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 3));
  EXPECT_EQ(2,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 4));
  EXPECT_EQ(3,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 5));
  EXPECT_EQ(4,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 6));
  EXPECT_EQ(5,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 7));
  EXPECT_EQ(6,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 8));
  EXPECT_EQ(7,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 9));
  EXPECT_EQ(7,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 10));
  EXPECT_EQ(7,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 11));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.y() + 2));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 2));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 3));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.y() + 2));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 2));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 3));
}

TEST_P(TilingDataTest, LastBorderTileYIndexFromSrcCoord) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 2));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 3));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 4));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 5));
  EXPECT_EQ(2,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 6));
  EXPECT_EQ(2,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 7));
  EXPECT_EQ(2,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 8));
  EXPECT_EQ(3,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 9));
  EXPECT_EQ(3,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 10));
  EXPECT_EQ(3,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            false,
                            origin.y() + 11));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(2,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 2));
  EXPECT_EQ(3,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 3));
  EXPECT_EQ(4,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 4));
  EXPECT_EQ(5,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 5));
  EXPECT_EQ(6,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 6));
  EXPECT_EQ(7,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 7));
  EXPECT_EQ(7,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 8));
  EXPECT_EQ(7,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 9));
  EXPECT_EQ(7,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 10));
  EXPECT_EQ(7,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(10, 10)),
                            true,
                            origin.y() + 11));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            false,
                            origin.y() + 2));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 2));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            false,
                            origin.y() + 3));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(1, 1),
                            gfx::Rect(origin, gfx::Size(1, 1)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2),
                            gfx::Rect(origin, gfx::Size(2, 2)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 3)),
                            true,
                            origin.y() + 2));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 0));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 1));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 2));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3),
                            gfx::Rect(origin, gfx::Size(3, 4)),
                            true,
                            origin.y() + 3));
}

TEST_P(TilingDataTest, TileSizeX) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(
      5, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(5, 5)), false, 0));
  EXPECT_EQ(
      5, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(5, 5)), true, 0));

  EXPECT_EQ(
      5, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), false, 0));
  EXPECT_EQ(
      1, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), false, 1));
  EXPECT_EQ(
      4, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), true, 0));
  EXPECT_EQ(
      2, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), true, 1));

  EXPECT_EQ(
      5, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), false, 0));
  EXPECT_EQ(
      3, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), false, 1));
  EXPECT_EQ(
      4, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), true, 0));
  EXPECT_EQ(
      4, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), true, 1));

  EXPECT_EQ(
      5,
      SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), false, 0));
  EXPECT_EQ(
      5,
      SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), false, 1));
  EXPECT_EQ(
      4, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), true, 0));
  EXPECT_EQ(
      3, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), true, 1));
  EXPECT_EQ(
      3, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), true, 2));

  EXPECT_EQ(
      4, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(11, 11)), true, 2));
  EXPECT_EQ(
      3, SizeX(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(12, 12)), true, 2));

  EXPECT_EQ(
      3, SizeX(gfx::Size(5, 9), gfx::Rect(origin, gfx::Size(12, 17)), true, 2));
}

TEST_P(TilingDataTest, TileSizeY) {
  gfx::Point origin = GetParam();

  EXPECT_EQ(
      5, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(5, 5)), false, 0));
  EXPECT_EQ(
      5, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(5, 5)), true, 0));

  EXPECT_EQ(
      5, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), false, 0));
  EXPECT_EQ(
      1, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), false, 1));
  EXPECT_EQ(
      4, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), true, 0));
  EXPECT_EQ(
      2, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(6, 6)), true, 1));

  EXPECT_EQ(
      5, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), false, 0));
  EXPECT_EQ(
      3, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), false, 1));
  EXPECT_EQ(
      4, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), true, 0));
  EXPECT_EQ(
      4, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(8, 8)), true, 1));

  EXPECT_EQ(
      5,
      SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), false, 0));
  EXPECT_EQ(
      5,
      SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), false, 1));
  EXPECT_EQ(
      4, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), true, 0));
  EXPECT_EQ(
      3, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), true, 1));
  EXPECT_EQ(
      3, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(10, 10)), true, 2));

  EXPECT_EQ(
      4, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(11, 11)), true, 2));
  EXPECT_EQ(
      3, SizeY(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(12, 12)), true, 2));

  EXPECT_EQ(
      3, SizeY(gfx::Size(9, 5), gfx::Rect(origin, gfx::Size(17, 12)), true, 2));
}

TEST_P(TilingDataTest, TileSizeX_and_TilePositionX) {
  gfx::Point origin = GetParam();

  // Single tile cases:
  EXPECT_EQ(
      1, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), false, 0));
  EXPECT_EQ(
      origin.x(),
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), false, 0));
  EXPECT_EQ(
      1,
      SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 100)), false, 0));
  EXPECT_EQ(
      origin.x(),
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 100)), false, 0));
  EXPECT_EQ(
      3, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 1)), false, 0));
  EXPECT_EQ(
      origin.x(),
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 1)), false, 0));
  EXPECT_EQ(
      3,
      SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 100)), false, 0));
  EXPECT_EQ(
      origin.x(),
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 100)), false, 0));
  EXPECT_EQ(
      1, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), true, 0));
  EXPECT_EQ(origin.x(),
            PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), true, 0));
  EXPECT_EQ(
      1, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 100)), true, 0));
  EXPECT_EQ(
      origin.x(),
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 100)), true, 0));
  EXPECT_EQ(
      3, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 1)), true, 0));
  EXPECT_EQ(origin.x(),
            PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 1)), true, 0));
  EXPECT_EQ(
      3, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 100)), true, 0));
  EXPECT_EQ(
      origin.x(),
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(3, 100)), true, 0));

  // Multiple tiles:
  // no border
  // positions 0, 3
  EXPECT_EQ(
      2, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), false));
  EXPECT_EQ(
      3, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), false, 0));
  EXPECT_EQ(
      3, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), false, 1));
  EXPECT_EQ(
      origin.x() + 0,
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), false, 0));
  EXPECT_EQ(
      origin.x() + 3,
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), false, 1));
  EXPECT_EQ(
      3,
      SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 100)), false, 0));
  EXPECT_EQ(
      3,
      SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 100)), false, 1));
  EXPECT_EQ(
      origin.x() + 0,
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 100)), false, 0));
  EXPECT_EQ(
      origin.x() + 3,
      PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 100)), false, 1));

  // Multiple tiles:
  // with border
  // positions 0, 2, 3, 4
  EXPECT_EQ(
      4, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true));
  EXPECT_EQ(
      2, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 0));
  EXPECT_EQ(
      1, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 1));
  EXPECT_EQ(
      1, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 2));
  EXPECT_EQ(
      2, SizeX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 3));
  EXPECT_EQ(origin.x() + 0,
            PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 0));
  EXPECT_EQ(origin.x() + 2,
            PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 1));
  EXPECT_EQ(origin.x() + 3,
            PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 2));
  EXPECT_EQ(origin.x() + 4,
            PosX(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(6, 1)), true, 3));
  EXPECT_EQ(
      2, SizeX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 0));
  EXPECT_EQ(
      1, SizeX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 1));
  EXPECT_EQ(
      1, SizeX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 2));
  EXPECT_EQ(
      2, SizeX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 3));
  EXPECT_EQ(
      origin.x() + 0,
      PosX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 0));
  EXPECT_EQ(
      origin.x() + 2,
      PosX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 1));
  EXPECT_EQ(
      origin.x() + 3,
      PosX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 2));
  EXPECT_EQ(
      origin.x() + 4,
      PosX(gfx::Size(3, 7), gfx::Rect(origin, gfx::Size(6, 100)), true, 3));
}

TEST_P(TilingDataTest, TileSizeY_and_TilePositionY) {
  gfx::Point origin = GetParam();

  // Single tile cases:
  EXPECT_EQ(
      1, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), false, 0));
  EXPECT_EQ(
      origin.y(),
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), false, 0));
  EXPECT_EQ(
      1,
      SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 1)), false, 0));
  EXPECT_EQ(
      origin.y(),
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 1)), false, 0));
  EXPECT_EQ(
      3, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 3)), false, 0));
  EXPECT_EQ(
      origin.y(),
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 3)), false, 0));
  EXPECT_EQ(
      3,
      SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 3)), false, 0));
  EXPECT_EQ(
      origin.y(),
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 3)), false, 0));
  EXPECT_EQ(
      1, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), true, 0));
  EXPECT_EQ(origin.y(),
            PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 1)), true, 0));
  EXPECT_EQ(
      1, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 1)), true, 0));
  EXPECT_EQ(
      origin.y(),
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 1)), true, 0));
  EXPECT_EQ(
      3, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 3)), true, 0));
  EXPECT_EQ(origin.y(),
            PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 3)), true, 0));
  EXPECT_EQ(
      3, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 3)), true, 0));
  EXPECT_EQ(
      origin.y(),
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 3)), true, 0));

  // Multiple tiles:
  // no border
  // positions 0, 3
  EXPECT_EQ(
      2, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), false));
  EXPECT_EQ(
      3, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), false, 0));
  EXPECT_EQ(
      3, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), false, 1));
  EXPECT_EQ(
      origin.y() + 0,
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), false, 0));
  EXPECT_EQ(
      origin.y() + 3,
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), false, 1));
  EXPECT_EQ(
      3,
      SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 6)), false, 0));
  EXPECT_EQ(
      3,
      SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 6)), false, 1));
  EXPECT_EQ(
      origin.y() + 0,
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 6)), false, 0));
  EXPECT_EQ(
      origin.y() + 3,
      PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(100, 6)), false, 1));

  // Multiple tiles:
  // with border
  // positions 0, 2, 3, 4
  EXPECT_EQ(
      4, NumTiles(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true));
  EXPECT_EQ(
      2, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 0));
  EXPECT_EQ(
      1, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 1));
  EXPECT_EQ(
      1, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 2));
  EXPECT_EQ(
      2, SizeY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 3));
  EXPECT_EQ(origin.y() + 0,
            PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 0));
  EXPECT_EQ(origin.y() + 2,
            PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 1));
  EXPECT_EQ(origin.y() + 3,
            PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 2));
  EXPECT_EQ(origin.y() + 4,
            PosY(gfx::Size(3, 3), gfx::Rect(origin, gfx::Size(1, 6)), true, 3));
  EXPECT_EQ(
      2, SizeY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 0));
  EXPECT_EQ(
      1, SizeY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 1));
  EXPECT_EQ(
      1, SizeY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 2));
  EXPECT_EQ(
      2, SizeY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 3));
  EXPECT_EQ(
      origin.y() + 0,
      PosY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 0));
  EXPECT_EQ(
      origin.y() + 2,
      PosY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 1));
  EXPECT_EQ(
      origin.y() + 3,
      PosY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 2));
  EXPECT_EQ(
      origin.y() + 4,
      PosY(gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(100, 6)), true, 3));
}

TEST_P(TilingDataTest, SetTotalSize) {
  gfx::Point origin = GetParam();

  TilingData data(gfx::Size(5, 5), gfx::Rect(origin, gfx::Size(5, 5)), false);
  EXPECT_EQ(origin.x(), data.tiling_rect().x());
  EXPECT_EQ(origin.y(), data.tiling_rect().y());
  EXPECT_EQ(5, data.tiling_rect().width());
  EXPECT_EQ(5, data.tiling_rect().height());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(1, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));

  data.SetTilingRect(gfx::Rect(36, 82, 6, 5));
  EXPECT_EQ(36, data.tiling_rect().x());
  EXPECT_EQ(82, data.tiling_rect().y());
  EXPECT_EQ(6, data.tiling_rect().width());
  EXPECT_EQ(5, data.tiling_rect().height());
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(1, data.TileSizeX(1));
  EXPECT_EQ(1, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));

  data.SetTilingRect(gfx::Rect(4, 22, 5, 12));
  EXPECT_EQ(4, data.tiling_rect().x());
  EXPECT_EQ(22, data.tiling_rect().y());
  EXPECT_EQ(5, data.tiling_rect().width());
  EXPECT_EQ(12, data.tiling_rect().height());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(3, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));
  EXPECT_EQ(5, data.TileSizeY(1));
  EXPECT_EQ(2, data.TileSizeY(2));
}

TEST_P(TilingDataTest, SetMaxTextureSizeNoBorders) {
  gfx::Point origin = GetParam();

  TilingData data(gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(16, 32)), false);
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(4, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(32, 32));
  EXPECT_EQ(gfx::Size(32, 32), data.max_texture_size());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(1, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), data.max_texture_size());
  EXPECT_EQ(8, data.num_tiles_x());
  EXPECT_EQ(16, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(5, 5), data.max_texture_size());
  EXPECT_EQ(4, data.num_tiles_x());
  EXPECT_EQ(7, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(8, 5));
  EXPECT_EQ(gfx::Size(8, 5), data.max_texture_size());
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(7, data.num_tiles_y());
}

TEST_P(TilingDataTest, SetMaxTextureSizeBorders) {
  gfx::Point origin = GetParam();

  TilingData data(gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(16, 32)), true);
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(5, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(32, 32));
  EXPECT_EQ(gfx::Size(32, 32), data.max_texture_size());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(1, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), data.max_texture_size());
  EXPECT_EQ(0, data.num_tiles_x());
  EXPECT_EQ(0, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(5, 5), data.max_texture_size());
  EXPECT_EQ(5, data.num_tiles_x());
  EXPECT_EQ(10, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(8, 5));
  EXPECT_EQ(gfx::Size(8, 5), data.max_texture_size());
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(10, data.num_tiles_y());
}

TEST_P(TilingDataTest, ExpandRectToTileBoundsWithBordersEmpty) {
  gfx::Point origin = GetParam();
  TilingData empty_total_size(
      gfx::Size(0, 0), gfx::Rect(origin, gfx::Size(8, 8)), true);
  EXPECT_RECT_EQ(
      gfx::Rect(),
      empty_total_size.ExpandRectToTileBoundsWithBorders(gfx::Rect()));
  EXPECT_RECT_EQ(gfx::Rect(),
                 empty_total_size.ExpandRectToTileBoundsWithBorders(
                     gfx::Rect(100, 100, 100, 100)));
  EXPECT_RECT_EQ(gfx::Rect(),
                 empty_total_size.ExpandRectToTileBoundsWithBorders(
                     gfx::Rect(0, 0, 100, 100)));

  TilingData empty_max_texture_size(
      gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(0, 0)), true);
  EXPECT_RECT_EQ(
      gfx::Rect(),
      empty_max_texture_size.ExpandRectToTileBoundsWithBorders(gfx::Rect()));
  EXPECT_RECT_EQ(gfx::Rect(),
                 empty_max_texture_size.ExpandRectToTileBoundsWithBorders(
                     gfx::Rect(100, 100, 100, 100)));
  EXPECT_RECT_EQ(gfx::Rect(),
                 empty_max_texture_size.ExpandRectToTileBoundsWithBorders(
                     gfx::Rect(0, 0, 100, 100)));
}

TEST_P(TilingDataTest, ExpandRectToTileBoundsWithBorders) {
  gfx::Point origin = GetParam();
  TilingData data(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(16, 32)), true);

  // Small rect at origin rounds up to tile 0, 0.
  gfx::Rect at_origin_src(origin, gfx::Size(1, 1));
  gfx::Rect at_origin_result(data.TileBoundsWithBorder(0, 0));
  EXPECT_NE(at_origin_src, at_origin_result);
  EXPECT_RECT_EQ(at_origin_result,
                 data.ExpandRectToTileBoundsWithBorders(at_origin_src));

  // Arbitrary internal rect.
  gfx::Rect rect_src(origin.x() + 6, origin.y() + 6, 1, 3);
  // Tile 2, 2 => gfx::Rect(4, 4, 4, 4)
  // Tile 3, 4 => gfx::Rect(6, 8, 4, 4)
  gfx::Rect rect_result(gfx::UnionRects(data.TileBoundsWithBorder(2, 2),
                                        data.TileBoundsWithBorder(3, 4)));
  EXPECT_NE(rect_src, rect_result);
  EXPECT_RECT_EQ(rect_result, data.ExpandRectToTileBoundsWithBorders(rect_src));

  // On tile bounds rounds up to next tile (since border overlaps).
  gfx::Rect border_rect_src(
      gfx::UnionRects(data.TileBounds(1, 2), data.TileBounds(3, 4)));
  gfx::Rect border_rect_result(gfx::UnionRects(
      data.TileBoundsWithBorder(0, 1), data.TileBoundsWithBorder(4, 5)));
  EXPECT_RECT_EQ(border_rect_result,
                 data.ExpandRectToTileBoundsWithBorders(border_rect_src));

  // Equal to tiling rect.
  EXPECT_RECT_EQ(data.tiling_rect(),
                 data.ExpandRectToTileBoundsWithBorders(data.tiling_rect()));

  // Containing, but larger than tiling rect.
  EXPECT_RECT_EQ(data.tiling_rect(),
                 data.ExpandRectToTileBoundsWithBorders(
                     gfx::Rect(origin, gfx::Size(100, 100))));

  // Non-intersecting with tiling rect.
  gfx::Rect non_intersect(origin.x() + 200, origin.y() + 200, 100, 100);
  EXPECT_FALSE(non_intersect.Intersects(data.tiling_rect()));
  EXPECT_RECT_EQ(gfx::Rect(),
                 data.ExpandRectToTileBoundsWithBorders(non_intersect));

  TilingData data2(gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(32, 64)), true);

  // Inside other tile border texels doesn't include other tiles.
  gfx::Rect inner_rect_src(data2.TileBounds(1, 1));
  inner_rect_src.Inset(data2.border_texels(), data.border_texels());
  gfx::Rect inner_rect_result(data2.TileBoundsWithBorder(1, 1));
  gfx::Rect expanded = data2.ExpandRectToTileBoundsWithBorders(inner_rect_src);
  EXPECT_EQ(inner_rect_result.ToString(), expanded.ToString());
}

TEST_P(TilingDataTest, ExpandRectToTileBounds) {
  gfx::Point origin = GetParam();
  TilingData data(gfx::Size(4, 4), gfx::Rect(origin, gfx::Size(16, 32)), true);

  // Small rect at origin rounds up to tile 0, 0.
  gfx::Rect at_origin_src(origin, gfx::Size(1, 1));
  gfx::Rect at_origin_result(data.TileBounds(0, 0));
  EXPECT_NE(at_origin_src, at_origin_result);
  EXPECT_RECT_EQ(at_origin_result, data.ExpandRectToTileBounds(at_origin_src));

  // Arbitrary internal rect.
  gfx::Rect rect_src(origin.x() + 6, origin.y() + 6, 1, 3);
  // Tile 2, 2 => gfx::Rect(4, 4, 4, 4)
  // Tile 3, 4 => gfx::Rect(6, 8, 4, 4)
  gfx::Rect rect_result(
      gfx::UnionRects(data.TileBounds(2, 2), data.TileBounds(3, 4)));
  EXPECT_NE(rect_src, rect_result);
  EXPECT_RECT_EQ(rect_result, data.ExpandRectToTileBounds(rect_src));

  // On tile bounds rounds up to next tile (since border overlaps).
  gfx::Rect border_rect_src(
      gfx::UnionRects(data.TileBounds(1, 2), data.TileBounds(3, 4)));
  gfx::Rect border_rect_result(
      gfx::UnionRects(data.TileBounds(0, 1), data.TileBounds(4, 5)));
  EXPECT_RECT_EQ(border_rect_result,
                 data.ExpandRectToTileBounds(border_rect_src));

  // Equal to tiling rect.
  EXPECT_RECT_EQ(data.tiling_rect(),
                 data.ExpandRectToTileBounds(data.tiling_rect()));

  // Containing, but larger than tiling rect.
  EXPECT_RECT_EQ(
      data.tiling_rect(),
      data.ExpandRectToTileBounds(gfx::Rect(origin, gfx::Size(100, 100))));

  // Non-intersecting with tiling rect.
  gfx::Rect non_intersect(origin.x() + 200, origin.y() + 200, 100, 100);
  EXPECT_FALSE(non_intersect.Intersects(data.tiling_rect()));
  EXPECT_RECT_EQ(gfx::Rect(), data.ExpandRectToTileBounds(non_intersect));

  TilingData data2(gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(32, 64)), true);

  // Inside other tile border texels doesn't include other tiles.
  gfx::Rect inner_rect_src(data2.TileBounds(1, 1));
  inner_rect_src.Inset(data2.border_texels(), data.border_texels());
  gfx::Rect inner_rect_result(data2.TileBounds(1, 1));
  gfx::Rect expanded = data2.ExpandRectToTileBounds(inner_rect_src);
  EXPECT_EQ(inner_rect_result.ToString(), expanded.ToString());
}

TEST_P(TilingDataTest, Assignment) {
  gfx::Point origin = GetParam();

  {
    TilingData source(
        gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(16, 32)), true);
    TilingData dest = source;
    EXPECT_EQ(source.border_texels(), dest.border_texels());
    EXPECT_EQ(source.max_texture_size(), dest.max_texture_size());
    EXPECT_EQ(source.num_tiles_x(), dest.num_tiles_x());
    EXPECT_EQ(source.num_tiles_y(), dest.num_tiles_y());
    EXPECT_EQ(source.tiling_rect().x(), dest.tiling_rect().x());
    EXPECT_EQ(source.tiling_rect().y(), dest.tiling_rect().y());
    EXPECT_EQ(source.tiling_rect().width(), dest.tiling_rect().width());
    EXPECT_EQ(source.tiling_rect().height(), dest.tiling_rect().height());
  }
  {
    TilingData source(
        gfx::Size(7, 3), gfx::Rect(origin, gfx::Size(6, 100)), false);
    TilingData dest(source);
    EXPECT_EQ(source.border_texels(), dest.border_texels());
    EXPECT_EQ(source.max_texture_size(), dest.max_texture_size());
    EXPECT_EQ(source.num_tiles_x(), dest.num_tiles_x());
    EXPECT_EQ(source.num_tiles_y(), dest.num_tiles_y());
    EXPECT_EQ(source.tiling_rect().x(), dest.tiling_rect().x());
    EXPECT_EQ(source.tiling_rect().y(), dest.tiling_rect().y());
    EXPECT_EQ(source.tiling_rect().width(), dest.tiling_rect().width());
    EXPECT_EQ(source.tiling_rect().height(), dest.tiling_rect().height());
  }
}

TEST_P(TilingDataTest, SetBorderTexels) {
  gfx::Point origin = GetParam();

  TilingData data(gfx::Size(8, 8), gfx::Rect(origin, gfx::Size(16, 32)), false);
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(4, data.num_tiles_y());

  data.SetHasBorderTexels(true);
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(5, data.num_tiles_y());

  data.SetHasBorderTexels(false);
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(4, data.num_tiles_y());
}

TEST_P(TilingDataTest, LargeBorders) {
  gfx::Point origin = GetParam();

  TilingData data(
      gfx::Size(100, 80), gfx::Rect(origin, gfx::Size(200, 145)), 30);
  EXPECT_EQ(30, data.border_texels());

  EXPECT_EQ(70, data.TileSizeX(0));
  EXPECT_EQ(40, data.TileSizeX(1));
  EXPECT_EQ(40, data.TileSizeX(2));
  EXPECT_EQ(50, data.TileSizeX(3));
  EXPECT_EQ(4, data.num_tiles_x());

  EXPECT_EQ(50, data.TileSizeY(0));
  EXPECT_EQ(20, data.TileSizeY(1));
  EXPECT_EQ(20, data.TileSizeY(2));
  EXPECT_EQ(20, data.TileSizeY(3));
  EXPECT_EQ(35, data.TileSizeY(4));
  EXPECT_EQ(5, data.num_tiles_y());

  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 0, origin.y() + 0, 70, 50),
                 data.TileBounds(0, 0));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 70, origin.y() + 50, 40, 20),
                 data.TileBounds(1, 1));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 110, origin.y() + 110, 40, 35),
                 data.TileBounds(2, 4));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 150, origin.y() + 70, 50, 20),
                 data.TileBounds(3, 2));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 150, origin.y() + 110, 50, 35),
                 data.TileBounds(3, 4));

  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 0, origin.y() + 0, 100, 80),
                 data.TileBoundsWithBorder(0, 0));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 40, origin.y() + 20, 100, 80),
                 data.TileBoundsWithBorder(1, 1));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 80, origin.y() + 80, 100, 65),
                 data.TileBoundsWithBorder(2, 4));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 120, origin.y() + 40, 80, 80),
                 data.TileBoundsWithBorder(3, 2));
  EXPECT_RECT_EQ(gfx::Rect(origin.x() + 120, origin.y() + 80, 80, 65),
                 data.TileBoundsWithBorder(3, 4));

  EXPECT_EQ(0, data.TileXIndexFromSrcCoord(origin.x() + 0));
  EXPECT_EQ(0, data.TileXIndexFromSrcCoord(origin.x() + 69));
  EXPECT_EQ(1, data.TileXIndexFromSrcCoord(origin.x() + 70));
  EXPECT_EQ(1, data.TileXIndexFromSrcCoord(origin.x() + 109));
  EXPECT_EQ(2, data.TileXIndexFromSrcCoord(origin.x() + 110));
  EXPECT_EQ(2, data.TileXIndexFromSrcCoord(origin.x() + 149));
  EXPECT_EQ(3, data.TileXIndexFromSrcCoord(origin.x() + 150));
  EXPECT_EQ(3, data.TileXIndexFromSrcCoord(origin.x() + 199));

  EXPECT_EQ(0, data.TileYIndexFromSrcCoord(origin.y() + 0));
  EXPECT_EQ(0, data.TileYIndexFromSrcCoord(origin.y() + 49));
  EXPECT_EQ(1, data.TileYIndexFromSrcCoord(origin.y() + 50));
  EXPECT_EQ(1, data.TileYIndexFromSrcCoord(origin.y() + 69));
  EXPECT_EQ(2, data.TileYIndexFromSrcCoord(origin.y() + 70));
  EXPECT_EQ(2, data.TileYIndexFromSrcCoord(origin.y() + 89));
  EXPECT_EQ(3, data.TileYIndexFromSrcCoord(origin.y() + 90));
  EXPECT_EQ(3, data.TileYIndexFromSrcCoord(origin.y() + 109));
  EXPECT_EQ(4, data.TileYIndexFromSrcCoord(origin.y() + 110));
  EXPECT_EQ(4, data.TileYIndexFromSrcCoord(origin.y() + 144));

  EXPECT_EQ(0, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 0));
  EXPECT_EQ(0, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 99));
  EXPECT_EQ(1, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 100));
  EXPECT_EQ(1, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 139));
  EXPECT_EQ(2, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 140));
  EXPECT_EQ(2, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 179));
  EXPECT_EQ(3, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 180));
  EXPECT_EQ(3, data.FirstBorderTileXIndexFromSrcCoord(origin.x() + 199));

  EXPECT_EQ(0, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 0));
  EXPECT_EQ(0, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 79));
  EXPECT_EQ(1, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 80));
  EXPECT_EQ(1, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 99));
  EXPECT_EQ(2, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 100));
  EXPECT_EQ(2, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 119));
  EXPECT_EQ(3, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 120));
  EXPECT_EQ(3, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 139));
  EXPECT_EQ(4, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 140));
  EXPECT_EQ(4, data.FirstBorderTileYIndexFromSrcCoord(origin.y() + 144));

  EXPECT_EQ(0, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 0));
  EXPECT_EQ(0, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 39));
  EXPECT_EQ(1, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 40));
  EXPECT_EQ(1, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 79));
  EXPECT_EQ(2, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 80));
  EXPECT_EQ(2, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 119));
  EXPECT_EQ(3, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 120));
  EXPECT_EQ(3, data.LastBorderTileXIndexFromSrcCoord(origin.x() + 199));

  EXPECT_EQ(0, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 0));
  EXPECT_EQ(0, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 19));
  EXPECT_EQ(1, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 20));
  EXPECT_EQ(1, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 39));
  EXPECT_EQ(2, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 40));
  EXPECT_EQ(2, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 59));
  EXPECT_EQ(3, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 60));
  EXPECT_EQ(3, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 79));
  EXPECT_EQ(4, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 80));
  EXPECT_EQ(4, data.LastBorderTileYIndexFromSrcCoord(origin.y() + 144));
}

void TestIterate(const TilingData& data,
                 gfx::Rect rect,
                 int expect_left,
                 int expect_top,
                 int expect_right,
                 int expect_bottom,
                 bool include_borders) {
  EXPECT_GE(expect_left, 0);
  EXPECT_GE(expect_top, 0);
  EXPECT_LT(expect_right, data.num_tiles_x());
  EXPECT_LT(expect_bottom, data.num_tiles_y());

  std::vector<std::pair<int, int> > original_expected;
  for (int x = 0; x < data.num_tiles_x(); ++x) {
    for (int y = 0; y < data.num_tiles_y(); ++y) {
      gfx::Rect bounds;
      if (include_borders)
        bounds = data.TileBoundsWithBorder(x, y);
      else
        bounds = data.TileBounds(x, y);
      if (x >= expect_left && x <= expect_right &&
          y >= expect_top && y <= expect_bottom) {
        EXPECT_TRUE(bounds.Intersects(rect));
        original_expected.push_back(std::make_pair(x, y));
      } else {
        EXPECT_FALSE(bounds.Intersects(rect));
      }
    }
  }

  // Verify with vanilla iterator.
  {
    std::vector<std::pair<int, int> > expected = original_expected;
    for (TilingData::Iterator iter(&data, rect, include_borders); iter;
         ++iter) {
      bool found = false;
      for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] == iter.index()) {
          expected[i] = expected.back();
          expected.pop_back();
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
    }
    EXPECT_EQ(0u, expected.size());
  }

  // Make sure this also works with a difference iterator and an empty ignore.
  // The difference iterator always includes borders, so ignore it otherwise.
  if (include_borders) {
    std::vector<std::pair<int, int> > expected = original_expected;
    for (TilingData::DifferenceIterator iter(&data, rect, gfx::Rect());
         iter; ++iter) {
      bool found = false;
      for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] == iter.index()) {
          expected[i] = expected.back();
          expected.pop_back();
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
    }
    EXPECT_EQ(0u, expected.size());
  }
}

void TestIterateBorders(const TilingData& data,
                        gfx::Rect rect,
                        int expect_left,
                        int expect_top,
                        int expect_right,
                        int expect_bottom) {
  bool include_borders = true;
  TestIterate(data,
              rect,
              expect_left,
              expect_top,
              expect_right,
              expect_bottom,
              include_borders);
}

void TestIterateNoBorders(const TilingData& data,
                          gfx::Rect rect,
                          int expect_left,
                          int expect_top,
                          int expect_right,
                          int expect_bottom) {
  bool include_borders = false;
  TestIterate(data,
              rect,
              expect_left,
              expect_top,
              expect_right,
              expect_bottom,
              include_borders);
}

void TestIterateAll(const TilingData& data,
                    gfx::Rect rect,
                    int expect_left,
                    int expect_top,
                    int expect_right,
                    int expect_bottom) {
  TestIterateBorders(
      data, rect, expect_left, expect_top, expect_right, expect_bottom);
  TestIterateNoBorders(
      data, rect, expect_left, expect_top, expect_right, expect_bottom);
}

TEST_P(TilingDataTest, IteratorNoBorderTexels) {
  gfx::Point origin = GetParam();

  TilingData data(
      gfx::Size(10, 10), gfx::Rect(origin, gfx::Size(40, 25)), false);
  // The following Coordinates are relative to the origin.
  // X border index by src coord: [0-10), [10-20), [20, 30), [30, 40)
  // Y border index by src coord: [0-10), [10-20), [20, 25)
  TestIterateAll(data, gfx::Rect(origin.x(), origin.y(), 40, 25), 0, 0, 3, 2);
  TestIterateAll(
      data, gfx::Rect(origin.x() + 15, origin.y() + 15, 8, 8), 1, 1, 2, 2);

  // Oversized.
  TestIterateAll(data,
                 gfx::Rect(origin.x() - 100, origin.y() - 100, 1000, 1000),
                 0,
                 0,
                 3,
                 2);
  TestIterateAll(
      data, gfx::Rect(origin.x() - 100, origin.y() + 20, 1000, 1), 0, 2, 3, 2);
  TestIterateAll(
      data, gfx::Rect(origin.x() + 29, origin.y() - 100, 31, 1000), 2, 0, 3, 2);
  // Nonintersecting.
  TestIterateAll(data,
                 gfx::Rect(origin.x() + 60, origin.y() + 80, 100, 100),
                 0,
                 0,
                 -1,
                 -1);
}

TEST_P(TilingDataTest, BordersIteratorOneBorderTexel) {
  gfx::Point origin = GetParam();

  TilingData data(
      gfx::Size(10, 20), gfx::Rect(origin, gfx::Size(25, 45)), true);
  // The following Coordinates are relative to the origin.
  // X border index by src coord: [0-10), [8-18), [16-25)
  // Y border index by src coord: [0-20), [18-38), [36-45)
  TestIterateBorders(
      data, gfx::Rect(origin.x(), origin.y(), 25, 45), 0, 0, 2, 2);
  TestIterateBorders(
      data, gfx::Rect(origin.x() + 18, origin.y() + 19, 3, 17), 2, 0, 2, 1);
  TestIterateBorders(
      data, gfx::Rect(origin.x() + 10, origin.y() + 20, 6, 16), 1, 1, 1, 1);
  TestIterateBorders(
      data, gfx::Rect(origin.x() + 9, origin.y() + 19, 8, 18), 0, 0, 2, 2);
  // Oversized.
  TestIterateBorders(data,
                     gfx::Rect(origin.x() - 100, origin.y() - 100, 1000, 1000),
                     0,
                     0,
                     2,
                     2);
  TestIterateBorders(
      data, gfx::Rect(origin.x() - 100, origin.y() + 20, 1000, 1), 0, 1, 2, 1);
  TestIterateBorders(
      data, gfx::Rect(origin.x() + 18, origin.y() - 100, 6, 1000), 2, 0, 2, 2);
  // Nonintersecting.
  TestIterateBorders(data,
                     gfx::Rect(origin.x() + 60, origin.y() + 80, 100, 100),
                     0,
                     0,
                     -1,
                     -1);
}

TEST_P(TilingDataTest, NoBordersIteratorOneBorderTexel) {
  gfx::Point origin = GetParam();

  TilingData data(
      gfx::Size(10, 20), gfx::Rect(origin, gfx::Size(25, 45)), true);
  // The following Coordinates are relative to the origin.
  // X index by src coord: [0-9), [9-17), [17-25)
  // Y index by src coord: [0-19), [19-37), [37-45)
  TestIterateNoBorders(
      data, gfx::Rect(origin.x(), origin.y(), 25, 45), 0, 0, 2, 2);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 17, origin.y() + 19, 3, 18), 2, 1, 2, 1);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 17, origin.y() + 19, 3, 19), 2, 1, 2, 2);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 8, origin.y() + 18, 9, 19), 0, 0, 1, 1);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 9, origin.y() + 19, 9, 19), 1, 1, 2, 2);
  // Oversized.
  TestIterateNoBorders(
      data,
      gfx::Rect(origin.x() - 100, origin.y() - 100, 1000, 1000),
      0,
      0,
      2,
      2);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() - 100, origin.y() + 20, 1000, 1), 0, 1, 2, 1);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 18, origin.y() - 100, 6, 1000), 2, 0, 2, 2);
  // Nonintersecting.
  TestIterateNoBorders(data,
                       gfx::Rect(origin.x() + 60, origin.y() + 80, 100, 100),
                       0,
                       0,
                       -1,
                       -1);
}

TEST_P(TilingDataTest, BordersIteratorManyBorderTexels) {
  gfx::Point origin = GetParam();

  TilingData data(gfx::Size(50, 60), gfx::Rect(origin, gfx::Size(65, 110)), 20);
  // The following Coordinates are relative to the origin.
  // X border index by src coord: [0-50), [10-60), [20-65)
  // Y border index by src coord: [0-60), [20-80), [40-100), [60-110)
  TestIterateBorders(
      data, gfx::Rect(origin.x(), origin.y(), 65, 110), 0, 0, 2, 3);
  TestIterateBorders(
      data, gfx::Rect(origin.x() + 50, origin.y() + 60, 15, 65), 1, 1, 2, 3);
  TestIterateBorders(
      data, gfx::Rect(origin.x() + 60, origin.y() + 30, 2, 10), 2, 0, 2, 1);
  // Oversized.
  TestIterateBorders(data,
                     gfx::Rect(origin.x() - 100, origin.y() - 100, 1000, 1000),
                     0,
                     0,
                     2,
                     3);
  TestIterateBorders(
      data, gfx::Rect(origin.x() - 100, origin.y() + 10, 1000, 10), 0, 0, 2, 0);
  TestIterateBorders(
      data, gfx::Rect(origin.x() + 10, origin.y() - 100, 10, 1000), 0, 0, 1, 3);
  // Nonintersecting.
  TestIterateBorders(data,
                     gfx::Rect(origin.x() + 65, origin.y() + 110, 100, 100),
                     0,
                     0,
                     -1,
                     -1);
}

TEST_P(TilingDataTest, NoBordersIteratorManyBorderTexels) {
  gfx::Point origin = GetParam();

  TilingData data(gfx::Size(50, 60), gfx::Rect(origin, gfx::Size(65, 110)), 20);
  // The following Coordinates are relative to the origin.
  // X index by src coord: [0-30), [30-40), [40, 65)
  // Y index by src coord: [0-40), [40-60), [60, 80), [80-110)
  TestIterateNoBorders(
      data, gfx::Rect(origin.x(), origin.y(), 65, 110), 0, 0, 2, 3);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 30, origin.y() + 40, 15, 65), 1, 1, 2, 3);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 60, origin.y() + 20, 2, 21), 2, 0, 2, 1);
  // Oversized.
  TestIterateNoBorders(
      data,
      gfx::Rect(origin.x() - 100, origin.y() - 100, 1000, 1000),
      0,
      0,
      2,
      3);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() - 100, origin.y() + 10, 1000, 10), 0, 0, 2, 0);
  TestIterateNoBorders(
      data, gfx::Rect(origin.x() + 10, origin.y() - 100, 10, 1000), 0, 0, 0, 3);
  // Nonintersecting.
  TestIterateNoBorders(data,
                       gfx::Rect(origin.x() + 65, origin.y() + 110, 100, 100),
                       0,
                       0,
                       -1,
                       -1);
}

TEST_P(TilingDataTest, IteratorOneTile) {
  gfx::Point origin = GetParam();

  TilingData no_border(
      gfx::Size(1000, 1000), gfx::Rect(origin, gfx::Size(30, 40)), false);
  TestIterateAll(
      no_border, gfx::Rect(origin.x(), origin.y(), 30, 40), 0, 0, 0, 0);
  TestIterateAll(no_border,
                 gfx::Rect(origin.x() + 10, origin.y() + 10, 20, 20),
                 0,
                 0,
                 0,
                 0);
  TestIterateAll(no_border,
                 gfx::Rect(origin.x() + 30, origin.y() + 40, 100, 100),
                 0,
                 0,
                 -1,
                 -1);

  TilingData one_border(
      gfx::Size(1000, 1000), gfx::Rect(origin, gfx::Size(30, 40)), true);
  TestIterateAll(
      one_border, gfx::Rect(origin.x(), origin.y(), 30, 40), 0, 0, 0, 0);
  TestIterateAll(one_border,
                 gfx::Rect(origin.x() + 10, origin.y() + 10, 20, 20),
                 0,
                 0,
                 0,
                 0);
  TestIterateAll(one_border,
                 gfx::Rect(origin.x() + 30, origin.y() + 40, 100, 100),
                 0,
                 0,
                 -1,
                 -1);

  TilingData big_border(
      gfx::Size(1000, 1000), gfx::Rect(origin, gfx::Size(30, 40)), 50);
  TestIterateAll(
      big_border, gfx::Rect(origin.x(), origin.y(), 30, 40), 0, 0, 0, 0);
  TestIterateAll(big_border,
                 gfx::Rect(origin.x() + 10, origin.y() + 10, 20, 20),
                 0,
                 0,
                 0,
                 0);
  TestIterateAll(big_border,
                 gfx::Rect(origin.x() + 30, origin.y() + 40, 100, 100),
                 0,
                 0,
                 -1,
                 -1);
}

TEST(TilingDataTest, IteratorNoTiles) {
  TilingData data(gfx::Size(100, 100), gfx::Rect(), false);
  TestIterateAll(data, gfx::Rect(0, 0, 100, 100), 0, 0, -1, -1);
}

void TestDiff(
    const TilingData& data,
    gfx::Rect consider,
    gfx::Rect ignore,
    size_t num_tiles) {

  std::vector<std::pair<int, int> > expected;
  for (int y = 0; y < data.num_tiles_y(); ++y) {
    for (int x = 0; x < data.num_tiles_x(); ++x) {
      gfx::Rect bounds = data.TileBoundsWithBorder(x, y);
      if (bounds.Intersects(consider) && !bounds.Intersects(ignore))
        expected.push_back(std::make_pair(x, y));
    }
  }

  // Sanity check the test.
  EXPECT_EQ(num_tiles, expected.size());

  for (TilingData::DifferenceIterator iter(&data, consider, ignore);
       iter; ++iter) {
    bool found = false;
    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] == iter.index()) {
        expected[i] = expected.back();
        expected.pop_back();
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
  EXPECT_EQ(0u, expected.size());
}

TEST_P(TilingDataTest, DifferenceIteratorIgnoreGeometry) {
  // This test is checking that the iterator can handle different geometries of
  // ignore rects relative to the consider rect.  The consider rect indices
  // themselves are mostly tested by the non-difference iterator tests, so the
  // full rect is mostly used here for simplicity.

  gfx::Point origin = GetParam();

  // The following Coordinates are relative to the origin.
  // X border index by src coord: [0-10), [10-20), [20, 30), [30, 40)
  // Y border index by src coord: [0-10), [10-20), [20, 25)
  TilingData data(
      gfx::Size(10, 10), gfx::Rect(origin, gfx::Size(40, 25)), false);

  // Fully ignored
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           0);
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() - 100, origin.y() - 100, 200, 200),
           0);
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() + 9, origin.y() + 9, 30, 15),
           0);
  TestDiff(data,
           gfx::Rect(origin.x() + 15, origin.y() + 15, 8, 8),
           gfx::Rect(origin.x() + 15, origin.y() + 15, 8, 8),
           0);

  // Fully un-ignored
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() - 30, origin.y() - 20, 8, 8),
           12);
  TestDiff(data, gfx::Rect(origin.x(), origin.y(), 40, 25), gfx::Rect(), 12);

  // Top left, remove 2x2 tiles
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x(), origin.y(), 20, 19),
           8);
  // Bottom right, remove 2x2 tiles
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() + 20, origin.y() + 15, 20, 6),
           8);
  // Bottom left, remove 2x2 tiles
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x(), origin.y() + 15, 20, 6),
           8);
  // Top right, remove 2x2 tiles
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() + 20, origin.y(), 20, 19),
           8);
  // Center, remove only one tile
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() + 10, origin.y() + 10, 5, 5),
           11);

  // Left column, flush left, removing two columns
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x(), origin.y(), 11, 25),
           6);
  // Middle column, removing two columns
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() + 11, origin.y(), 11, 25),
           6);
  // Right column, flush right, removing one column
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x() + 30, origin.y(), 2, 25),
           9);

  // Top row, flush top, removing one row
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x(), origin.y() + 5, 40, 5),
           8);
  // Middle row, removing one row
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x(), origin.y() + 13, 40, 5),
           8);
  // Bottom row, flush bottom, removing two rows
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 40, 25),
           gfx::Rect(origin.x(), origin.y() + 13, 40, 12),
           4);

  // Non-intersecting, but still touching two of the same tiles.
  TestDiff(data,
           gfx::Rect(origin.x() + 8, origin.y(), 32, 25),
           gfx::Rect(origin.x(), origin.y() + 12, 5, 12),
           10);

  // Intersecting, but neither contains the other. 2x3 with one overlap.
  TestDiff(data,
           gfx::Rect(origin.x() + 5, origin.y() + 2, 20, 10),
           gfx::Rect(origin.x() + 25, origin.y() + 15, 5, 10),
           5);
}

TEST_P(TilingDataTest, DifferenceIteratorManyBorderTexels) {
  gfx::Point origin = GetParam();

  // The following Coordinates are relative to the origin.
  // X border index by src coord: [0-50), [10-60), [20-65)
  // Y border index by src coord: [0-60), [20-80), [40-100), [60-110)
  TilingData data(gfx::Size(50, 60), gfx::Rect(origin, gfx::Size(65, 110)), 20);

  // Ignore one column, three rows
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y() + 30, 55, 80),
           gfx::Rect(origin.x() + 5, origin.y() + 30, 5, 15),
           9);

  // Knock out three columns, leaving only one.
  TestDiff(data,
           gfx::Rect(origin.x() + 10, origin.y() + 30, 55, 80),
           gfx::Rect(origin.x() + 30, origin.y() + 59, 20, 1),
           3);

  // Overlap all tiles with ignore rect.
  TestDiff(data,
           gfx::Rect(origin.x(), origin.y(), 65, 110),
           gfx::Rect(origin.x() + 30, origin.y() + 59, 1, 2),
           0);
}

TEST_P(TilingDataTest, DifferenceIteratorOneTile) {
  gfx::Point origin = GetParam();

  TilingData no_border(
      gfx::Size(1000, 1000), gfx::Rect(origin, gfx::Size(30, 40)), false);
  TestDiff(
      no_border, gfx::Rect(origin.x(), origin.y(), 30, 40), gfx::Rect(), 1);
  TestDiff(no_border,
           gfx::Rect(origin.x() + 5, origin.y() + 5, 100, 100),
           gfx::Rect(origin.x() + 5, origin.y() + 5, 1, 1),
           0);

  TilingData one_border(
      gfx::Size(1000, 1000), gfx::Rect(origin, gfx::Size(30, 40)), true);
  TestDiff(
      one_border, gfx::Rect(origin.x(), origin.y(), 30, 40), gfx::Rect(), 1);
  TestDiff(one_border,
           gfx::Rect(origin.x() + 5, origin.y() + 5, 100, 100),
           gfx::Rect(origin.x() + 5, origin.y() + 5, 1, 1),
           0);

  TilingData big_border(
      gfx::Size(1000, 1000), gfx::Rect(origin, gfx::Size(30, 40)), 50);
  TestDiff(
      big_border, gfx::Rect(origin.x(), origin.y(), 30, 40), gfx::Rect(), 1);
  TestDiff(big_border,
           gfx::Rect(origin.x() + 5, origin.y() + 5, 100, 100),
           gfx::Rect(origin.x() + 5, origin.y() + 5, 1, 1),
           0);
}

TEST(TilingDataTest, DifferenceIteratorNoTiles) {
  TilingData data(gfx::Size(100, 100), gfx::Rect(), false);
  TestDiff(data, gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 0, 5, 5), 0);
}

void TestSpiralIterate(int source_line_number,
                       const TilingData& tiling_data,
                       const gfx::Rect& consider,
                       const gfx::Rect& ignore,
                       const gfx::Rect& center,
                       const std::vector<std::pair<int, int> >& expected) {
  std::vector<std::pair<int, int> > actual;
  for (TilingData::SpiralDifferenceIterator it(
           &tiling_data, consider, ignore, center);
       it;
       ++it) {
    actual.push_back(it.index());
  }

  EXPECT_EQ(expected.size(), actual.size()) << "error from line "
                                            << source_line_number;
  for (size_t i = 0; i < std::min(expected.size(), actual.size()); ++i) {
    EXPECT_EQ(expected[i].first, actual[i].first)
        << "i: " << i << " error from line: " << source_line_number;
    EXPECT_EQ(expected[i].second, actual[i].second)
        << "i: " << i << " error from line: " << source_line_number;
  }
}

TEST_P(TilingDataTest, SpiralDifferenceIteratorNoIgnoreFullConsider) {
  gfx::Point origin = GetParam();
  TilingData tiling_data(
      gfx::Size(10, 10), gfx::Rect(origin, gfx::Size(30, 30)), false);
  gfx::Rect consider(origin.x(), origin.y(), 30, 30);
  gfx::Rect ignore;
  std::vector<std::pair<int, int> > expected;

  // Center is in the center of the tiling.
  gfx::Rect center(origin.x() + 15, origin.y() + 15, 1, 1);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| 4 3 2
  //  1| 5 * 1
  //  2| 6 7 8
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is off to the right side of the tiling (and far away).
  center = gfx::Rect(origin.x() + 100, origin.y() + 15, 1, 1);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| 7 4 1
  //  1| 8 5 2 *
  //  2| 9 6 3
  expected.clear();
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is the bottom right corner of the tiling.
  center = gfx::Rect(origin.x() + 25, origin.y() + 25, 1, 1);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| 6 5 4
  //  1| 7 2 1
  //  2| 8 3 *
  expected.clear();
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is off the top left side of the tiling.
  center = gfx::Rect(origin.x() - 60, origin.y() - 50, 1, 1);

  // Layout of the tiling data, and expected return order:
  // * x 0 1 2
  //  y.------
  //  0| 1 2 6
  //  1| 3 4 5
  //  2| 7 8 9
  expected.clear();
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Two tile center.
  center = gfx::Rect(origin.x() + 15, origin.y() + 15, 1, 10);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| 5 4 3
  //  1| 6 * 2
  //  2| 7 * 1
  expected.clear();
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST_P(TilingDataTest, SpiralDifferenceIteratorSmallConsider) {
  gfx::Point origin = GetParam();
  TilingData tiling_data(
      gfx::Size(10, 10), gfx::Rect(origin, gfx::Size(50, 50)), false);
  gfx::Rect ignore;
  std::vector<std::pair<int, int> > expected;
  gfx::Rect center(origin.x() + 15, origin.y() + 15, 1, 1);

  // Consider is one cell.
  gfx::Rect consider(origin.x(), origin.y(), 1, 1);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0| 1
  //  1|   *
  //  2|
  //  3|
  //  4|
  expected.push_back(std::make_pair(0, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider is bottom right corner.
  consider = gfx::Rect(origin.x() + 25, origin.y() + 25, 10, 10);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0|
  //  1|   *
  //  2|     1 2
  //  3|     3 4
  //  4|
  expected.clear();
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(3, 2));
  expected.push_back(std::make_pair(2, 3));
  expected.push_back(std::make_pair(3, 3));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider is one column.
  consider = gfx::Rect(origin.x() + 11, origin.y(), 1, 100);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0|   2
  //  1|   *
  //  2|   3
  //  3|   4
  //  4|   5
  expected.clear();
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(1, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST_P(TilingDataTest, SpiralDifferenceIteratorHasIgnore) {
  gfx::Point origin = GetParam();
  TilingData tiling_data(
      gfx::Size(10, 10), gfx::Rect(origin, gfx::Size(50, 50)), false);
  gfx::Rect consider(origin.x(), origin.y(), 50, 50);
  std::vector<std::pair<int, int> > expected;
  gfx::Rect center(origin.x() + 15, origin.y() + 15, 1, 1);

  // Full ignore.
  gfx::Rect ignore(origin.x(), origin.y(), 50, 50);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0| . . . . .
  //  1| . * . . .
  //  2| . . . . .
  //  3| . . . . .
  //  4| . . . . .
  expected.clear();

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // 3 column ignore.
  ignore = gfx::Rect(origin.x() + 15, origin.y(), 20, 100);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0| 1 . . . 8
  //  1| 2 * . . 7
  //  2| 3 . . . 6
  //  3| 4 . . . 5
  //  4| 9 . . . 10
  expected.clear();

  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(4, 0));
  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(4, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Ignore covers the top half.
  ignore = gfx::Rect(origin.x(), origin.y(), 50, 25);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0| . . . . .
  //  1| . * . . .
  //  2| . . . . .
  //  3| 1 2 3 4 5
  //  4| 6 7 8 9 10
  expected.clear();

  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(2, 3));
  expected.push_back(std::make_pair(3, 3));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(4, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST_P(TilingDataTest, SpiralDifferenceIteratorRectangleCenter) {
  gfx::Point origin = GetParam();
  TilingData tiling_data(
      gfx::Size(10, 10), gfx::Rect(origin, gfx::Size(50, 50)), false);
  gfx::Rect consider(origin.x(), origin.y(), 50, 50);
  std::vector<std::pair<int, int> > expected;
  gfx::Rect ignore;

  // Two cell center
  gfx::Rect center(origin.x() + 25, origin.y() + 25, 1, 10);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0| J I H G F
  //  1| K 5 4 3 E
  //  2| L 6 * 2 D
  //  3| M 7 * 1 C
  //  4| N 8 9 A B
  expected.clear();

  expected.push_back(std::make_pair(3, 3));
  expected.push_back(std::make_pair(3, 2));
  expected.push_back(std::make_pair(3, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(4, 4));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(4, 0));
  expected.push_back(std::make_pair(3, 0));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(0, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Three by two center.
  center = gfx::Rect(origin.x() + 15, origin.y() + 25, 20, 10);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2 3 4
  //  y.----------
  //  0| J I H G F
  //  1| 7 6 5 4 3
  //  2| 8 * * * 2
  //  3| 9 * * * 1
  //  4| A B C D E
  expected.clear();

  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(3, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(4, 4));
  expected.push_back(std::make_pair(4, 0));
  expected.push_back(std::make_pair(3, 0));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Column center off the left side.
  center = gfx::Rect(origin.x() - 50, origin.y(), 30, 50);

  // Layout of the tiling data, and expected return order:
  //    x 0 1 2 3 4
  //   y.----------
  // * 0| 5 A F K P
  // * 1| 4 9 E J O
  // * 2| 3 8 D I N
  // * 3| 2 7 C H M
  // * 4| 1 6 B G L
  expected.clear();

  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(2, 3));
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(3, 3));
  expected.push_back(std::make_pair(3, 2));
  expected.push_back(std::make_pair(3, 1));
  expected.push_back(std::make_pair(3, 0));
  expected.push_back(std::make_pair(4, 4));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(4, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST_P(TilingDataTest, SpiralDifferenceIteratorEdgeCases) {
  gfx::Point origin = GetParam();
  TilingData tiling_data(
      gfx::Size(10, 10), gfx::Rect(origin, gfx::Size(30, 30)), false);
  std::vector<std::pair<int, int> > expected;
  gfx::Rect center;
  gfx::Rect consider;
  gfx::Rect ignore;

  // Ignore contains, but is not equal to, consider and center.
  ignore = gfx::Rect(origin.x() + 15, origin.y(), 20, 30);
  consider = gfx::Rect(origin.x() + 20, origin.y() + 10, 10, 20);
  center = gfx::Rect(origin.x() + 25, origin.y(), 5, 5);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0|   . *
  //  1|   . .
  //  2|   . .
  expected.clear();

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center intersects with consider.
  ignore = gfx::Rect();
  center = gfx::Rect(origin.x(), origin.y() + 15, 30, 15);
  consider = gfx::Rect(origin.x(), origin.y(), 15, 30);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| 2 1
  //  1| * * *
  //  2| * * *
  expected.clear();

  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider and ignore are non-intersecting.
  ignore = gfx::Rect(origin.x(), origin.y(), 5, 30);
  consider = gfx::Rect(origin.x() + 25, origin.y(), 5, 30);
  center = gfx::Rect(origin.x() + 15, origin.y(), 1, 1);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| . * 1
  //  1| .   2
  //  2| .   3
  expected.clear();

  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center intersects with ignore.
  consider = gfx::Rect(origin.x(), origin.y(), 30, 30);
  center = gfx::Rect(origin.x() + 15, origin.y(), 1, 30);
  ignore = gfx::Rect(origin.x(), origin.y() + 15, 30, 1);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| 3 * 2
  //  1| . * .
  //  2| 4 * 1
  expected.clear();

  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center and ignore are the same.
  consider = gfx::Rect(origin.x(), origin.y(), 30, 30);
  center = gfx::Rect(origin.x() + 15, origin.y(), 1, 30);
  ignore = center;

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  //  0| 4 * 3
  //  1| 5 * 2
  //  2| 6 * 1
  expected.clear();

  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Empty tiling data.
  TilingData empty_data(gfx::Size(0, 0), gfx::Rect(0, 0, 0, 0), false);

  expected.clear();
  TestSpiralIterate(__LINE__, empty_data, consider, ignore, center, expected);

  // Empty consider.
  ignore = gfx::Rect();
  center = gfx::Rect(1, 1, 1, 1);
  consider = gfx::Rect();

  expected.clear();
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Empty center. Note: This arbitrarily puts the center to be off the top-left
  // corner.
  consider = gfx::Rect(origin.x(), origin.y(), 30, 30);
  ignore = gfx::Rect();
  center = gfx::Rect();

  // Layout of the tiling data, and expected return order:
  // * x 0 1 2
  //  y.------
  //  0| 1 2 6
  //  1| 3 4 5
  //  2| 7 8 9
  expected.clear();

  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Every rect is empty.
  ignore = gfx::Rect();
  center = gfx::Rect();
  consider = gfx::Rect();

  expected.clear();
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is just to the left of cover, and off of the tiling's left side.
  consider = gfx::Rect(origin.x(), origin.y(), 30, 30);
  ignore = gfx::Rect();
  center = gfx::Rect(origin.x() - 20, origin.y(), 19, 30);

  // Layout of the tiling data, and expected return order:
  //   x 0 1 2
  //  y.------
  // *0| 3 6 9
  // *1| 2 5 8
  // *2| 1 4 7
  expected.clear();

  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

INSTANTIATE_TEST_CASE_P(TilingData,
                        TilingDataTest,
                        ::testing::Values(gfx::Point(42, 17),
                                          gfx::Point(0, 0),
                                          gfx::Point(-8, 15),
                                          gfx::Point(-12, 4),
                                          gfx::Point(-16, -35),
                                          gfx::Point(-10000, -15000)));
}  // namespace

}  // namespace cc
