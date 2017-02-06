// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/single_split_view.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/single_split_view_listener.h"

namespace {

static void VerifySplitViewLayout(const views::SingleSplitView& split) {
  ASSERT_EQ(2, split.child_count());

  const views::View* leading = split.child_at(0);
  const views::View* trailing = split.child_at(1);

  if (split.bounds().IsEmpty()) {
    EXPECT_TRUE(leading->bounds().IsEmpty());
    EXPECT_TRUE(trailing->bounds().IsEmpty());
    return;
  }

  EXPECT_FALSE(leading->bounds().IsEmpty());
  EXPECT_FALSE(trailing->bounds().IsEmpty());
  EXPECT_FALSE(leading->bounds().Intersects(trailing->bounds()));

  if (split.orientation() == views::SingleSplitView::HORIZONTAL_SPLIT) {
    EXPECT_EQ(leading->bounds().height(),
              split.bounds().height() - split.GetInsets().height());
    EXPECT_EQ(trailing->bounds().height(),
              split.bounds().height() - split.GetInsets().height());
    EXPECT_LT(leading->bounds().width() + trailing->bounds().width(),
              split.bounds().width() - split.GetInsets().width());
  } else if (split.orientation() == views::SingleSplitView::VERTICAL_SPLIT) {
    EXPECT_EQ(leading->bounds().width(),
              split.bounds().width() - split.GetInsets().width());
    EXPECT_EQ(trailing->bounds().width(),
              split.bounds().width() - split.GetInsets().width());
    EXPECT_LT(leading->bounds().height() + trailing->bounds().height(),
              split.bounds().height() - split.GetInsets().height());
  } else {
    NOTREACHED();
  }
}

class SingleSplitViewListenerImpl : public views::SingleSplitViewListener {
 public:
  SingleSplitViewListenerImpl() : count_(0) {}

  bool SplitHandleMoved(views::SingleSplitView* sender) override {
    ++count_;
    return false;
  }

  int count() const { return count_; }

 private:
  int count_;

  DISALLOW_COPY_AND_ASSIGN(SingleSplitViewListenerImpl);
};

class MinimumSizedView: public views::View {
 public:
  MinimumSizedView(gfx::Size min_size) : min_size_(min_size) {}

 private:
  gfx::Size min_size_;
  gfx::Size GetMinimumSize() const override;
};

gfx::Size MinimumSizedView::GetMinimumSize() const {
  return min_size_;
}

}  // namespace

namespace views {

TEST(SingleSplitViewTest, Resize) {
  // Test cases to iterate through for horizontal and vertical split views.
  struct TestCase {
    // Split view resize policy for this test case.
    bool resize_leading_on_bounds_change;
    // Split view size to set.
    int primary_axis_size;
    int secondary_axis_size;
    // Expected divider offset.
    int divider_offset;
  } test_cases[] = {
    // The initial split size is 100x100, divider at 33.
    { true, 100, 100, 33 },
    // Grow the split view, leading view should grow.
    { true, 1000, 100, 933 },
    // Shrink the split view, leading view should shrink.
    { true, 200, 100, 133 },
    // Minimize the split view, divider should not move.
    { true, 0, 0, 133 },
    // Restore the split view, divider should not move.
    { false, 500, 100, 133 },
    // Resize the split view by secondary axis, divider should not move.
    { false,  500, 600, 133 }
  };

  SingleSplitView::Orientation orientations[] = {
    SingleSplitView::HORIZONTAL_SPLIT,
    SingleSplitView::VERTICAL_SPLIT
  };

  int borders[][4] = {
      {0, 0, 0, 0}, {5, 5, 5, 5}, {10, 5, 5, 0},
  };

  for (size_t orientation = 0; orientation < arraysize(orientations);
       ++orientation) {
    for (size_t border = 0; border < arraysize(borders); ++border) {
      // Create a split view.
      SingleSplitView split(new View(), new View(), orientations[orientation],
                            NULL);

      // Set initial size and divider offset.
      EXPECT_EQ(test_cases[0].primary_axis_size,
                test_cases[0].secondary_axis_size);
      split.SetBounds(0, 0, test_cases[0].primary_axis_size,
                      test_cases[0].secondary_axis_size);

      if (border != 0)
        split.SetBorder(
            Border::CreateEmptyBorder(borders[border][0], borders[border][1],
                                      borders[border][2], borders[border][3]));

      split.set_divider_offset(test_cases[0].divider_offset);
      split.Layout();

      // Run all test cases.
      for (size_t i = 0; i < arraysize(test_cases); ++i) {
        split.set_resize_leading_on_bounds_change(
            test_cases[i].resize_leading_on_bounds_change);
        if (split.orientation() == SingleSplitView::HORIZONTAL_SPLIT) {
          split.SetBounds(0, 0, test_cases[i].primary_axis_size,
                          test_cases[i].secondary_axis_size);
        } else {
          split.SetBounds(0, 0, test_cases[i].secondary_axis_size,
                          test_cases[i].primary_axis_size);
        }

        EXPECT_EQ(test_cases[i].divider_offset, split.divider_offset());
        VerifySplitViewLayout(split);
      }

      // Special cases, one of the child views is hidden.
      split.child_at(0)->SetVisible(false);
      split.Layout();

      EXPECT_EQ(split.GetContentsBounds().size(), split.child_at(1)->size());

      split.child_at(0)->SetVisible(true);
      split.child_at(1)->SetVisible(false);
      split.Layout();

      EXPECT_EQ(split.GetContentsBounds().size(), split.child_at(0)->size());
    }
  }
}

TEST(SingleSplitViewTest, MouseDrag) {
  const int kMinimumChildSize = 25;
  MinimumSizedView *child0 =
      new MinimumSizedView(gfx::Size(5, kMinimumChildSize));
  MinimumSizedView *child1 =
      new MinimumSizedView(gfx::Size(5, kMinimumChildSize));
  SingleSplitViewListenerImpl listener;
  SingleSplitView split(
      child0, child1, SingleSplitView::VERTICAL_SPLIT, &listener);

  const int kTotalSplitSize = 100;
  split.SetBounds(0, 0, 10, kTotalSplitSize);
  const int kInitialDividerOffset = 33;
  const int kMouseOffset = 2;  // Mouse offset in the divider.
  const int kMouseMoveDelta = 7;
  split.set_divider_offset(kInitialDividerOffset);
  split.Layout();

  gfx::Point press_point(7, kInitialDividerOffset + kMouseOffset);
  ui::MouseEvent mouse_pressed(ui::ET_MOUSE_PRESSED, press_point, press_point,
                               ui::EventTimeForNow(), 0, 0);
  ASSERT_TRUE(split.OnMousePressed(mouse_pressed));
  EXPECT_EQ(kInitialDividerOffset, split.divider_offset());
  EXPECT_EQ(0, listener.count());

  // Drag divider to the bottom.
  gfx::Point drag_1_point(
      5, kInitialDividerOffset + kMouseOffset + kMouseMoveDelta);
  ui::MouseEvent mouse_dragged_1(ui::ET_MOUSE_DRAGGED, drag_1_point,
                                 drag_1_point, ui::EventTimeForNow(), 0, 0);
  ASSERT_TRUE(split.OnMouseDragged(mouse_dragged_1));
  EXPECT_EQ(kInitialDividerOffset + kMouseMoveDelta, split.divider_offset());
  EXPECT_EQ(1, listener.count());

  // Drag divider to the top, beyond first child minimum size.
  gfx::Point drag_2_point(
      7, kMinimumChildSize - 5);
  ui::MouseEvent mouse_dragged_2(ui::ET_MOUSE_DRAGGED, drag_2_point,
                                 drag_2_point, ui::EventTimeForNow(), 0, 0);
  ASSERT_TRUE(split.OnMouseDragged(mouse_dragged_2));
  EXPECT_EQ(kMinimumChildSize, split.divider_offset());
  EXPECT_EQ(2, listener.count());

  // Drag divider to the bottom, beyond second child minimum size.
  gfx::Point drag_3_point(
      7, kTotalSplitSize - kMinimumChildSize + 5);
  ui::MouseEvent mouse_dragged_3(ui::ET_MOUSE_DRAGGED, drag_3_point,
                                 drag_3_point, ui::EventTimeForNow(), 0, 0);
  ASSERT_TRUE(split.OnMouseDragged(mouse_dragged_3));
  EXPECT_EQ(kTotalSplitSize - kMinimumChildSize - split.GetDividerSize(),
            split.divider_offset());
  EXPECT_EQ(3, listener.count());

  // Drag divider between childs' minimum sizes.
  gfx::Point drag_4_point(
      6, kInitialDividerOffset + kMouseOffset + kMouseMoveDelta * 2);
  ui::MouseEvent mouse_dragged_4(ui::ET_MOUSE_DRAGGED, drag_4_point,
                                 drag_4_point, ui::EventTimeForNow(), 0, 0);
  ASSERT_TRUE(split.OnMouseDragged(mouse_dragged_4));
  EXPECT_EQ(kInitialDividerOffset + kMouseMoveDelta * 2,
            split.divider_offset());
  EXPECT_EQ(4, listener.count());

  gfx::Point release_point(
      7, kInitialDividerOffset + kMouseOffset + kMouseMoveDelta * 2);
  ui::MouseEvent mouse_released(ui::ET_MOUSE_RELEASED, release_point,
                                release_point, ui::EventTimeForNow(), 0, 0);
  split.OnMouseReleased(mouse_released);
  EXPECT_EQ(kInitialDividerOffset + kMouseMoveDelta * 2,
            split.divider_offset());

  // Expect intial offset after a system/user gesture cancels the drag.
  // This shouldn't occur after mouse release, but it's sufficient for testing.
  split.OnMouseCaptureLost();
  EXPECT_EQ(kInitialDividerOffset, split.divider_offset());
  EXPECT_EQ(5, listener.count());
}

}  // namespace views
