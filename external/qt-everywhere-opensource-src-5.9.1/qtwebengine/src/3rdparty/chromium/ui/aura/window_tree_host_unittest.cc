// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace aura {

using WindowTreeHostTest = test::AuraTestBase;

TEST_F(WindowTreeHostTest, DPIWindowSize) {
  gfx::Rect starting_bounds(0, 0, 800, 600);
  EXPECT_EQ(starting_bounds.size(), host()->compositor()->size());
  EXPECT_EQ(starting_bounds, host()->GetBounds());
  EXPECT_EQ(starting_bounds, root_window()->bounds());

  test_screen()->SetDeviceScaleFactor(1.5f);
  EXPECT_EQ(starting_bounds, host()->GetBounds());
  // Size should be rounded up after scaling.
  EXPECT_EQ(gfx::Rect(0, 0, 534, 400), root_window()->bounds());

  gfx::Transform transform;
  transform.Translate(0, 1.1f);
  host()->SetRootTransform(transform);
  EXPECT_EQ(gfx::Rect(0, 1, 534, 401), root_window()->bounds());

  gfx::Insets padding(1, 2, 3, 4);
  // Padding is in physical pixels.
  host()->SetOutputSurfacePadding(padding);
  gfx::Rect padded_rect = starting_bounds;
  padded_rect.Inset(-padding);
  EXPECT_EQ(padded_rect.size(), host()->compositor()->size());
  EXPECT_EQ(starting_bounds, host()->GetBounds());
  EXPECT_EQ(gfx::Rect(1, 1, 534, 401), root_window()->bounds());
  EXPECT_EQ(gfx::Vector2dF(0, 0),
            host()->compositor()->root_layer()->subpixel_position_offset());
}

}  // namespace aura
