// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_finder.h"

#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/window_finder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {
namespace ws {

TEST(WindowFinderTest, FindDeepestVisibleWindow) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  EnableHitTest(&child1);
  child1.SetVisible(true);
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  EnableHitTest(&child2);
  child2.SetVisible(true);
  child2.SetBounds(gfx::Rect(15, 15, 20, 20));

  gfx::Point local_point(16, 16);
  EXPECT_EQ(&child2, FindDeepestVisibleWindowForEvents(&root, &local_point));
  EXPECT_EQ(gfx::Point(1, 1), local_point);

  local_point.SetPoint(13, 14);
  EXPECT_EQ(&child1, FindDeepestVisibleWindowForEvents(&root, &local_point));
  EXPECT_EQ(gfx::Point(3, 4), local_point);

  child2.set_extended_hit_test_region(gfx::Insets(10, 10, 10, 10));
  local_point.SetPoint(13, 14);
  EXPECT_EQ(&child2, FindDeepestVisibleWindowForEvents(&root, &local_point));
  EXPECT_EQ(gfx::Point(-2, -1), local_point);
}

TEST(WindowFinderTest, FindDeepestVisibleWindowHitTestMask) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  EnableHitTest(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  ServerWindow child_with_mask(&window_delegate, WindowId(1, 4));
  root.Add(&child_with_mask);
  EnableHitTest(&child_with_mask);
  child_with_mask.SetVisible(true);
  child_with_mask.SetBounds(gfx::Rect(10, 10, 20, 20));
  child_with_mask.SetHitTestMask(gfx::Rect(2, 2, 16, 16));

  // Test a point inside the window but outside the mask.
  gfx::Point point_outside_mask(11, 11);
  EXPECT_EQ(&root,
            FindDeepestVisibleWindowForEvents(&root, &point_outside_mask));
  EXPECT_EQ(gfx::Point(11, 11), point_outside_mask);

  // Test a point inside the window and inside the mask.
  gfx::Point point_inside_mask(15, 15);
  EXPECT_EQ(&child_with_mask,
            FindDeepestVisibleWindowForEvents(&root, &point_inside_mask));
  EXPECT_EQ(gfx::Point(5, 5), point_inside_mask);
}

}  // namespace ws
}  // namespace mus
