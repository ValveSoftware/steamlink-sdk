// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/lap_timer.h"
#include "cc/resources/picture_layer_tiling.h"
#include "cc/test/fake_picture_layer_tiling_client.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PictureLayerTilingPerfTest : public testing::Test {
 public:
  PictureLayerTilingPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  virtual void SetUp() OVERRIDE {
    picture_layer_tiling_client_.SetTileSize(gfx::Size(256, 256));
    picture_layer_tiling_client_.set_max_tiles_for_interest_area(250);
    picture_layer_tiling_ = PictureLayerTiling::Create(
        1, gfx::Size(256 * 50, 256 * 50), &picture_layer_tiling_client_);
    picture_layer_tiling_->CreateAllTilesForTesting();
  }

  virtual void TearDown() OVERRIDE {
    picture_layer_tiling_.reset(NULL);
  }

  void RunInvalidateTest(const std::string& test_name, const Region& region) {
    timer_.Reset();
    do {
      picture_layer_tiling_->Invalidate(region);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult(
        "invalidation", "", test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunUpdateTilePrioritiesStationaryTest(const std::string& test_name,
                                             const gfx::Transform& transform) {
    gfx::Rect viewport_rect(0, 0, 1024, 768);

    timer_.Reset();
    do {
      picture_layer_tiling_->UpdateTilePriorities(
          ACTIVE_TREE, viewport_rect, 1.f, timer_.NumLaps() + 1);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("update_tile_priorities_stationary",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunUpdateTilePrioritiesScrollingTest(const std::string& test_name,
                                            const gfx::Transform& transform) {
    gfx::Size viewport_size(1024, 768);
    gfx::Rect viewport_rect(viewport_size);
    int xoffsets[] = {10, 0, -10, 0};
    int yoffsets[] = {0, 10, 0, -10};
    int offsetIndex = 0;
    int offsetCount = 0;
    const int maxOffsetCount = 1000;

    timer_.Reset();
    do {
      picture_layer_tiling_->UpdateTilePriorities(
          ACTIVE_TREE, viewport_rect, 1.f, timer_.NumLaps() + 1);

      viewport_rect = gfx::Rect(viewport_rect.x() + xoffsets[offsetIndex],
                                viewport_rect.y() + yoffsets[offsetIndex],
                                viewport_rect.width(),
                                viewport_rect.height());

      if (++offsetCount > maxOffsetCount) {
        offsetCount = 0;
        offsetIndex = (offsetIndex + 1) % 4;
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("update_tile_priorities_scrolling",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunTilingRasterTileIteratorTest(const std::string& test_name,
                                       int num_tiles,
                                       const gfx::Rect& viewport) {
    gfx::Size bounds(10000, 10000);
    picture_layer_tiling_ =
        PictureLayerTiling::Create(1, bounds, &picture_layer_tiling_client_);
    picture_layer_tiling_->UpdateTilePriorities(
        ACTIVE_TREE, viewport, 1.0f, 1.0);

    timer_.Reset();
    do {
      int count = num_tiles;
      for (PictureLayerTiling::TilingRasterTileIterator it(
               picture_layer_tiling_.get(), ACTIVE_TREE);
           it && count;
           ++it) {
        --count;
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_raster_tile_iterator",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

 private:
  FakePictureLayerTilingClient picture_layer_tiling_client_;
  scoped_ptr<PictureLayerTiling> picture_layer_tiling_;

  LapTimer timer_;
};

TEST_F(PictureLayerTilingPerfTest, Invalidate) {
  Region one_tile(gfx::Rect(256, 256));
  RunInvalidateTest("1x1", one_tile);

  Region half_region(gfx::Rect(25 * 256, 50 * 256));
  RunInvalidateTest("25x50", half_region);

  Region full_region(gfx::Rect(50 * 256, 50 * 256));
  RunInvalidateTest("50x50", full_region);
}

#if defined(OS_ANDROID)
// TODO(vmpstr): Investigate why this is noisy (crbug.com/310220).
TEST_F(PictureLayerTilingPerfTest, DISABLED_UpdateTilePriorities) {
#else
TEST_F(PictureLayerTilingPerfTest, UpdateTilePriorities) {
#endif  // defined(OS_ANDROID)
  gfx::Transform transform;

  RunUpdateTilePrioritiesStationaryTest("no_transform", transform);
  RunUpdateTilePrioritiesScrollingTest("no_transform", transform);

  transform.Rotate(10);
  RunUpdateTilePrioritiesStationaryTest("rotation", transform);
  RunUpdateTilePrioritiesScrollingTest("rotation", transform);

  transform.ApplyPerspectiveDepth(10);
  RunUpdateTilePrioritiesStationaryTest("perspective", transform);
  RunUpdateTilePrioritiesScrollingTest("perspective", transform);
}

TEST_F(PictureLayerTilingPerfTest, TilingRasterTileIterator) {
  RunTilingRasterTileIteratorTest("32_100x100", 32, gfx::Rect(0, 0, 100, 100));
  RunTilingRasterTileIteratorTest("32_500x500", 32, gfx::Rect(0, 0, 500, 500));
  RunTilingRasterTileIteratorTest("64_100x100", 64, gfx::Rect(0, 0, 100, 100));
  RunTilingRasterTileIteratorTest("64_500x500", 64, gfx::Rect(0, 0, 500, 500));
}

}  // namespace

}  // namespace cc
