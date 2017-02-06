// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/test/test_views.h"

#if defined(OS_MACOSX)
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

namespace views {

namespace {

const int kWidth = 100;
const int kMinHeight = 50;
const int kMaxHeight = 100;

enum ScrollBarOrientation { HORIZONTAL, VERTICAL };

// View implementation that allows setting the preferred size.
class CustomView : public View {
 public:
  CustomView() {}

  void SetPreferredSize(const gfx::Size& size) {
    preferred_size_ = size;
    PreferredSizeChanged();
  }

  gfx::Size GetPreferredSize() const override { return preferred_size_; }

  void Layout() override {
    gfx::Size pref = GetPreferredSize();
    int width = pref.width();
    int height = pref.height();
    if (parent()) {
      width = std::max(parent()->width(), width);
      height = std::max(parent()->height(), height);
    }
    SetBounds(x(), y(), width, height);
  }

 private:
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(CustomView);
};

void CheckScrollbarVisibility(const ScrollView& scroll_view,
                              ScrollBarOrientation orientation,
                              bool should_be_visible) {
  const ScrollBar* scrollbar = orientation == HORIZONTAL
                                   ? scroll_view.horizontal_scroll_bar()
                                   : scroll_view.vertical_scroll_bar();
  if (should_be_visible) {
    ASSERT_TRUE(scrollbar);
    EXPECT_TRUE(scrollbar->visible());
  } else {
    EXPECT_TRUE(!scrollbar || !scrollbar->visible());
  }
}

}  // namespace

// Verifies the viewport is sized to fit the available space.
TEST(ScrollViewTest, ViewportSizedToFit) {
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  scroll_view.Layout();
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the scrollbars are added as necessary.
// If on Mac, test the non-overlay scrollbars.
TEST(ScrollViewTest, ScrollBars) {
#if defined(OS_MACOSX)
  ui::test::ScopedPreferredScrollerStyle scroller_style_override(false);
#endif

  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view.Layout();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  CheckScrollbarVisibility(scroll_view, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view, HORIZONTAL, false);
  EXPECT_TRUE(!scroll_view.horizontal_scroll_bar() ||
              !scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight(),
            contents->parent()->height());
  CheckScrollbarVisibility(scroll_view, VERTICAL, false);
  CheckScrollbarVisibility(scroll_view, HORIZONTAL, true);

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view.Layout();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight(),
            contents->parent()->height());
  CheckScrollbarVisibility(scroll_view, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view, HORIZONTAL, true);

  // Add a border, test vertical scrollbar.
  const int kTopPadding = 1;
  const int kLeftPadding = 2;
  const int kBottomPadding = 3;
  const int kRightPadding = 4;
  scroll_view.SetBorder(Border::CreateEmptyBorder(
      kTopPadding, kLeftPadding, kBottomPadding, kRightPadding));
  contents->SetBounds(0, 0, 50, 400);
  scroll_view.Layout();
  EXPECT_EQ(
      100 - scroll_view.GetScrollBarWidth() - kLeftPadding - kRightPadding,
      contents->parent()->width());
  EXPECT_EQ(100 - kTopPadding - kBottomPadding, contents->parent()->height());
  EXPECT_TRUE(!scroll_view.horizontal_scroll_bar() ||
              !scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());
  gfx::Rect bounds = scroll_view.vertical_scroll_bar()->bounds();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth() - kRightPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(kTopPadding, bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());

  // Horizontal with border.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view.Layout();
  EXPECT_EQ(100 - kLeftPadding - kRightPadding, contents->parent()->width());
  EXPECT_EQ(
      100 - scroll_view.GetScrollBarHeight() - kTopPadding - kBottomPadding,
      contents->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  EXPECT_TRUE(!scroll_view.vertical_scroll_bar() ||
              !scroll_view.vertical_scroll_bar()->visible());
  bounds = scroll_view.horizontal_scroll_bar()->bounds();
  EXPECT_EQ(kLeftPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(100 - kBottomPadding - scroll_view.GetScrollBarHeight(),
            bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());

  // Both horizontal and vertical with border.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view.Layout();
  EXPECT_EQ(
      100 - scroll_view.GetScrollBarWidth() - kLeftPadding - kRightPadding,
      contents->parent()->width());
  EXPECT_EQ(
      100 - scroll_view.GetScrollBarHeight() - kTopPadding - kBottomPadding,
      contents->parent()->height());
  bounds = scroll_view.horizontal_scroll_bar()->bounds();
  // Check horiz.
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  bounds = scroll_view.horizontal_scroll_bar()->bounds();
  EXPECT_EQ(kLeftPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding - scroll_view.GetScrollBarWidth(),
            bounds.right());
  EXPECT_EQ(100 - kBottomPadding - scroll_view.GetScrollBarHeight(),
            bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());
  // Check vert.
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());
  bounds = scroll_view.vertical_scroll_bar()->bounds();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth() - kRightPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(kTopPadding, bounds.y());
  EXPECT_EQ(100 - kBottomPadding - scroll_view.GetScrollBarHeight(),
            bounds.bottom());
}

// Assertions around adding a header.
TEST(ScrollViewTest, Header) {
  ScrollView scroll_view;
  View* contents = new View;
  CustomView* header = new CustomView;
  scroll_view.SetHeader(header);
  View* header_parent = header->parent();
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  scroll_view.Layout();
  // |header|s preferred size is empty, which should result in all space going
  // to contents.
  EXPECT_EQ("0,0 100x0", header->parent()->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());

  // Get the header a height of 20.
  header->SetPreferredSize(gfx::Size(10, 20));
  EXPECT_EQ("0,0 100x20", header->parent()->bounds().ToString());
  EXPECT_EQ("0,20 100x80", contents->parent()->bounds().ToString());

  // Remove the header.
  scroll_view.SetHeader(NULL);
  // SetHeader(NULL) deletes header.
  header = NULL;
  EXPECT_EQ("0,0 100x0", header_parent->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the scrollbars are added as necessary when a header is present.
TEST(ScrollViewTest, ScrollBarsWithHeader) {
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  CustomView* header = new CustomView;
  scroll_view.SetHeader(header);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  header->SetPreferredSize(gfx::Size(10, 20));

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(80, contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  EXPECT_TRUE(!scroll_view.horizontal_scroll_bar() ||
              !scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());
  // Make sure the vertical scrollbar overlaps the header.
  EXPECT_EQ(header->y(), scroll_view.vertical_scroll_bar()->y());
  EXPECT_EQ(header->y(), contents->y());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100, header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  EXPECT_TRUE(!scroll_view.vertical_scroll_bar() ||
              !scroll_view.vertical_scroll_bar()->visible());

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());
}

// Verifies the header scrolls horizontally with the content.
TEST(ScrollViewTest, HeaderScrollsWithContent) {
  ScrollView scroll_view;
  CustomView* contents = new CustomView;
  scroll_view.SetContents(contents);
  contents->SetPreferredSize(gfx::Size(500, 500));

  CustomView* header = new CustomView;
  scroll_view.SetHeader(header);
  header->SetPreferredSize(gfx::Size(500, 20));

  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ("0,0", contents->bounds().origin().ToString());
  EXPECT_EQ("0,0", header->bounds().origin().ToString());

  // Scroll the horizontal scrollbar.
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar());
  scroll_view.ScrollToPosition(
      const_cast<ScrollBar*>(scroll_view.horizontal_scroll_bar()), 1);
  EXPECT_EQ("-1,0", contents->bounds().origin().ToString());
  EXPECT_EQ("-1,0", header->bounds().origin().ToString());

  // Scrolling the vertical scrollbar shouldn't effect the header.
  ASSERT_TRUE(scroll_view.vertical_scroll_bar());
  scroll_view.ScrollToPosition(
      const_cast<ScrollBar*>(scroll_view.vertical_scroll_bar()), 1);
  EXPECT_EQ("-1,-1", contents->bounds().origin().ToString());
  EXPECT_EQ("-1,0", header->bounds().origin().ToString());
}

// Verifies ScrollRectToVisible() on the child works.
TEST(ScrollViewTest, ScrollRectToVisible) {
  ScrollView scroll_view;
  CustomView* contents = new CustomView;
  scroll_view.SetContents(contents);
  contents->SetPreferredSize(gfx::Size(500, 1000));

  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  scroll_view.Layout();
  EXPECT_EQ("0,0", contents->bounds().origin().ToString());

  // Scroll to y=405 height=10, this should make the y position of the content
  // at (405 + 10) - viewport_height (scroll region bottom aligned).
  contents->ScrollRectToVisible(gfx::Rect(0, 405, 10, 10));
  const int viewport_height = contents->parent()->height();
  EXPECT_EQ(-(415 - viewport_height), contents->y());

  // Scroll to the current y-location and 10x10; should do nothing.
  contents->ScrollRectToVisible(gfx::Rect(0, -contents->y(), 10, 10));
  EXPECT_EQ(-(415 - viewport_height), contents->y());
}

// Verifies ClipHeightTo() uses the height of the content when it is between the
// minimum and maximum height values.
TEST(ScrollViewTest, ClipHeightToNormalContentHeight) {
  ScrollView scroll_view;

  scroll_view.ClipHeightTo(kMinHeight, kMaxHeight);

  const int kNormalContentHeight = 75;
  scroll_view.SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kNormalContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroll_view.GetPreferredSize());

  scroll_view.SizeToPreferredSize();
  scroll_view.Layout();

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroll_view.contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight), scroll_view.size());
}

// Verifies ClipHeightTo() uses the minimum height when the content is shorter
// thamn the minimum height value.
TEST(ScrollViewTest, ClipHeightToShortContentHeight) {
  ScrollView scroll_view;

  scroll_view.ClipHeightTo(kMinHeight, kMaxHeight);

  const int kShortContentHeight = 10;
  scroll_view.SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kShortContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view.GetPreferredSize());

  scroll_view.SizeToPreferredSize();
  scroll_view.Layout();

  EXPECT_EQ(gfx::Size(kWidth, kShortContentHeight),
            scroll_view.contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view.size());
}

// Verifies ClipHeightTo() uses the maximum height when the content is longer
// thamn the maximum height value.
TEST(ScrollViewTest, ClipHeightToTallContentHeight) {
  ScrollView scroll_view;

  // Use a scrollbar that is disabled by default, so the width of the content is
  // not affected.
  scroll_view.SetVerticalScrollBar(new views::OverlayScrollBar(false));

  scroll_view.ClipHeightTo(kMinHeight, kMaxHeight);

  const int kTallContentHeight = 1000;
  scroll_view.SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kTallContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view.GetPreferredSize());

  scroll_view.SizeToPreferredSize();
  scroll_view.Layout();

  EXPECT_EQ(gfx::Size(kWidth, kTallContentHeight),
            scroll_view.contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view.size());
}

// Verifies that when ClipHeightTo() produces a scrollbar, it reduces the width
// of the inner content of the ScrollView.
TEST(ScrollViewTest, ClipHeightToScrollbarUsesWidth) {
  ScrollView scroll_view;

  scroll_view.ClipHeightTo(kMinHeight, kMaxHeight);

  // Create a view that will be much taller than it is wide.
  scroll_view.SetContents(new views::ProportionallySizedView(1000));

  // Without any width, it will default to 0,0 but be overridden by min height.
  scroll_view.SizeToPreferredSize();
  EXPECT_EQ(gfx::Size(0, kMinHeight), scroll_view.GetPreferredSize());

  gfx::Size new_size(kWidth, scroll_view.GetHeightForWidth(kWidth));
  scroll_view.SetSize(new_size);
  scroll_view.Layout();

  int scroll_bar_width = scroll_view.GetScrollBarWidth();
  int expected_width = kWidth - scroll_bar_width;
  EXPECT_EQ(scroll_view.contents()->size().width(), expected_width);
  EXPECT_EQ(scroll_view.contents()->size().height(), 1000 * expected_width);
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view.size());
}

TEST(ScrollViewTest, CornerViewVisibility) {
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  View* corner_view = scroll_view.corner_view_;

  // Corner view should be visible when both scrollbars are visible.
  contents->SetBounds(0, 0, 200, 200);
  scroll_view.Layout();
  EXPECT_EQ(&scroll_view, corner_view->parent());
  EXPECT_TRUE(corner_view->visible());

  // Corner view should be aligned to the scrollbars.
  EXPECT_EQ(scroll_view.vertical_scroll_bar()->x(), corner_view->x());
  EXPECT_EQ(scroll_view.horizontal_scroll_bar()->y(), corner_view->y());
  EXPECT_EQ(scroll_view.GetScrollBarWidth(), corner_view->width());
  EXPECT_EQ(scroll_view.GetScrollBarHeight(), corner_view->height());

  // Corner view should be removed when only the vertical scrollbar is visible.
  contents->SetBounds(0, 0, 50, 200);
  scroll_view.Layout();
  EXPECT_FALSE(corner_view->parent());

  // ... or when only the horizontal scrollbar is visible.
  contents->SetBounds(0, 0, 200, 50);
  scroll_view.Layout();
  EXPECT_FALSE(corner_view->parent());

  // ... or when no scrollbar is visible.
  contents->SetBounds(0, 0, 50, 50);
  scroll_view.Layout();
  EXPECT_FALSE(corner_view->parent());

  // Corner view should reappear when both scrollbars reappear.
  contents->SetBounds(0, 0, 200, 200);
  scroll_view.Layout();
  EXPECT_EQ(&scroll_view, corner_view->parent());
  EXPECT_TRUE(corner_view->visible());
}

#if defined(OS_MACOSX)
// Tests the overlay scrollbars on Mac. Ensure that they show up properly and
// do not overlap each other.
TEST(ScrollViewTest, CocoaOverlayScrollBars) {
  std::unique_ptr<ui::test::ScopedPreferredScrollerStyle>
      scroller_style_override;
  scroller_style_override.reset(
      new ui::test::ScopedPreferredScrollerStyle(true));
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  // Size the contents such that vertical scrollbar is needed.
  // Since it is overlaid, the ViewPort size should match the ScrollView.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view.GetScrollBarWidth());
  CheckScrollbarVisibility(scroll_view, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view, HORIZONTAL, false);

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view.GetScrollBarHeight());
  CheckScrollbarVisibility(scroll_view, VERTICAL, false);
  CheckScrollbarVisibility(scroll_view, HORIZONTAL, true);

  // Both horizontal and vertical scrollbars.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view.GetScrollBarWidth());
  EXPECT_EQ(0, scroll_view.GetScrollBarHeight());
  CheckScrollbarVisibility(scroll_view, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view, HORIZONTAL, true);

  // Make sure the horizontal and vertical scrollbars don't overlap each other.
  gfx::Rect vert_bounds = scroll_view.vertical_scroll_bar()->bounds();
  gfx::Rect horiz_bounds = scroll_view.horizontal_scroll_bar()->bounds();
  EXPECT_EQ(vert_bounds.x(), horiz_bounds.right());
  EXPECT_EQ(horiz_bounds.y(), vert_bounds.bottom());

  // Switch to the non-overlay style and check that the ViewPort is now sized
  // to be smaller, and ScrollbarWidth and ScrollbarHeight are non-zero.
  scroller_style_override.reset(
      new ui::test::ScopedPreferredScrollerStyle(false));
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight(),
            contents->parent()->height());
  EXPECT_NE(0, scroll_view.GetScrollBarWidth());
  EXPECT_NE(0, scroll_view.GetScrollBarHeight());
}
#endif

}  // namespace views
