// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_coordinate_conversions.h"

#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

namespace ws {

using WindowCoordinateConversionsTest = testing::Test;

TEST_F(WindowCoordinateConversionsTest, ConvertRectBetweenWindows) {
  TestServerWindowDelegate d1, d2, d3;
  ServerWindow v1(&d1, WindowId()), v2(&d2, WindowId()), v3(&d3, WindowId());
  v1.SetBounds(gfx::Rect(1, 2, 100, 100));
  v2.SetBounds(gfx::Rect(3, 4, 100, 100));
  v3.SetBounds(gfx::Rect(5, 6, 100, 100));
  v1.Add(&v2);
  v2.Add(&v3);

  EXPECT_EQ(gfx::Rect(2, 1, 8, 9),
            ConvertRectBetweenWindows(&v1, &v3, gfx::Rect(10, 11, 8, 9)));

  EXPECT_EQ(gfx::Rect(18, 21, 8, 9),
            ConvertRectBetweenWindows(&v3, &v1, gfx::Rect(10, 11, 8, 9)));
}

TEST_F(WindowCoordinateConversionsTest, ConvertPointFBetweenWindows) {
  TestServerWindowDelegate d1, d2, d3;
  ServerWindow v1(&d1, WindowId()), v2(&d2, WindowId()), v3(&d3, WindowId());
  v1.SetBounds(gfx::Rect(1, 2, 100, 100));
  v2.SetBounds(gfx::Rect(3, 4, 100, 100));
  v3.SetBounds(gfx::Rect(5, 6, 100, 100));
  v1.Add(&v2);
  v2.Add(&v3);

  {
    const gfx::PointF result(
        ConvertPointFBetweenWindows(&v1, &v3, gfx::PointF(10.5f, 11.9f)));
    EXPECT_FLOAT_EQ(2.5f, result.x());
    EXPECT_FLOAT_EQ(1.9f, result.y());
  }

  {
    const gfx::PointF result(
        ConvertPointFBetweenWindows(&v3, &v1, gfx::PointF(10.2f, 11.4f)));
    EXPECT_FLOAT_EQ(18.2f, result.x());
    EXPECT_FLOAT_EQ(21.4f, result.y());
  }
}

}  // namespace ws

}  // namespace ui
