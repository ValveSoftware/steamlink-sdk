// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/flood_fill_ink_drop_ripple.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/test/flood_fill_ink_drop_ripple_test_api.h"

namespace views {
namespace test {

TEST(FloodFillInkDropRippleTest, TransformedCenterPointForIrregularClipBounds) {
  const gfx::Rect clip_bounds(8, 9, 32, 32);
  const gfx::Point requested_center_point(25, 24);

  // |expected_center_point| is in the |clip_bounds| coordinate space.
  const gfx::Point expected_center_point(
      requested_center_point.x() - clip_bounds.x(),
      requested_center_point.y() - clip_bounds.y());

  FloodFillInkDropRipple ripple(clip_bounds, requested_center_point,
                                SK_ColorWHITE, 0.175f);
  FloodFillInkDropRippleTestApi test_api(&ripple);

  gfx::Point actual_center = test_api.GetDrawnCenterPoint();
  test_api.TransformPoint(10, &actual_center);

  EXPECT_EQ(expected_center_point, actual_center);
}

TEST(FloodFillInkDropRippleTest, MaxDistanceToCorners) {
  const float kAbsError = 0.01f;
  // Rect with the following corners in clockwise order starting at the origin:
  // (10, 30), (60, 30), (10, 100), (60, 100)
  const gfx::Rect clip_bounds(10, 30, 50, 70);

  FloodFillInkDropRipple ripple(clip_bounds, gfx::Point(), SK_ColorWHITE,
                                0.175f);
  FloodFillInkDropRippleTestApi test_api(&ripple);

  // Interior points
  EXPECT_NEAR(78.10f, test_api.MaxDistanceToCorners(gfx::Point(10, 40)),
              kAbsError);
  EXPECT_NEAR(71.06f, test_api.MaxDistanceToCorners(gfx::Point(55, 45)),
              kAbsError);
  EXPECT_NEAR(64.03f, test_api.MaxDistanceToCorners(gfx::Point(50, 80)),
              kAbsError);
  EXPECT_NEAR(68.01f, test_api.MaxDistanceToCorners(gfx::Point(20, 85)),
              kAbsError);

  // Exterior points
  EXPECT_NEAR(110.79f, test_api.MaxDistanceToCorners(gfx::Point(3, 5)),
              kAbsError);
  EXPECT_NEAR(108.17f, test_api.MaxDistanceToCorners(gfx::Point(70, 10)),
              kAbsError);
  EXPECT_NEAR(103.08f, test_api.MaxDistanceToCorners(gfx::Point(75, 110)),
              kAbsError);
  EXPECT_NEAR(101.24f, test_api.MaxDistanceToCorners(gfx::Point(5, 115)),
              kAbsError);
}

}  // namespace test
}  // namespace views
