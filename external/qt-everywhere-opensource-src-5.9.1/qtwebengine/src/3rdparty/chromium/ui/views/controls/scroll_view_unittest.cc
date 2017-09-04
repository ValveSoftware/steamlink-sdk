// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

#if defined(OS_MACOSX)
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

enum ScrollBarOrientation { HORIZONTAL, VERTICAL };

namespace views {
namespace test {

class ScrollViewTestApi {
 public:
  explicit ScrollViewTestApi(ScrollView* scroll_view)
      : scroll_view_(scroll_view) {}

  BaseScrollBar* GetBaseScrollBar(ScrollBarOrientation orientation) {
    ScrollBar* scroll_bar = orientation == VERTICAL ? scroll_view_->vert_sb_
                                                    : scroll_view_->horiz_sb_;
    return static_cast<BaseScrollBar*>(scroll_bar);
  }

  const base::Timer& GetScrollBarTimer(ScrollBarOrientation orientation) {
    return GetBaseScrollBar(orientation)->repeater_.timer_for_testing();
  }

  BaseScrollBarThumb* GetScrollBarThumb(ScrollBarOrientation orientation) {
    return GetBaseScrollBar(orientation)->thumb_;
  }

  gfx::Point IntegralViewOffset() {
    return gfx::Point() - gfx::ScrollOffsetToFlooredVector2d(CurrentOffset());
  }

  gfx::ScrollOffset CurrentOffset() { return scroll_view_->CurrentOffset(); }

  base::Timer* GetScrollBarHideTimer(ScrollBarOrientation orientation) {
    return BaseScrollBar::GetHideTimerForTest(GetBaseScrollBar(orientation));
  }

  View* corner_view() { return scroll_view_->corner_view_; }
  View* contents_viewport() { return scroll_view_->contents_viewport_; }

 private:
  ScrollView* scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(ScrollViewTestApi);
};

}  // namespace test

namespace {

const int kWidth = 100;
const int kMinHeight = 50;
const int kMaxHeight = 100;

// View implementation that allows setting the preferred size.
class CustomView : public View {
 public:
  CustomView() {}

  void SetPreferredSize(const gfx::Size& size) {
    preferred_size_ = size;
    PreferredSizeChanged();
  }

  const gfx::Point last_location() const { return last_location_; }

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

  bool OnMousePressed(const ui::MouseEvent& event) override {
    last_location_ = event.location();
    return true;
  }

 private:
  gfx::Size preferred_size_;
  gfx::Point last_location_;

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

ui::MouseEvent TestLeftMouseAt(const gfx::Point& location, ui::EventType type) {
  return ui::MouseEvent(type, location, location, base::TimeTicks(),
                        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
}

}  // namespace

using test::ScrollViewTestApi;

// Simple test harness for testing a ScrollView directly.
class ScrollViewTest : public ViewsTestBase {
 public:
  ScrollViewTest() {}

  View* InstallContents() {
    const gfx::Rect default_outer_bounds(0, 0, 100, 100);
    View* contents = new View;
    scroll_view_.SetContents(contents);
    scroll_view_.SetBoundsRect(default_outer_bounds);
    return contents;
  }

 protected:
#if defined(OS_MACOSX)
  void SetOverlayScrollersEnabled(bool enabled) {
    // Ensure the old scroller override is destroyed before creating a new one.
    // Otherwise, the swizzlers are interleaved and restore incorrect methods.
    scroller_style_.reset();
    scroller_style_ =
        base::MakeUnique<ui::test::ScopedPreferredScrollerStyle>(enabled);
  }

 private:
  // Disable overlay scrollers by default. This needs to be set before
  // |scroll_view_| is initialized, otherwise scrollers will try to animate to
  // change modes, which requires a MessageLoop to exist. Tests should only
  // modify this via SetOverlayScrollersEnabled().
  std::unique_ptr<ui::test::ScopedPreferredScrollerStyle> scroller_style_ =
      base::MakeUnique<ui::test::ScopedPreferredScrollerStyle>(false);

 protected:
#endif
  ScrollView scroll_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollViewTest);
};

// Test harness that includes a Widget to help test ui::Event handling.
class WidgetScrollViewTest : public test::WidgetTest,
                             public ui::CompositorObserver {
 public:
  static const int kDefaultHeight = 100;
  static const int kDefaultWidth = 100;

  WidgetScrollViewTest() {}

  // Call this before adding the ScrollView to test with overlay scrollbars.
  void SetUseOverlayScrollers() {
    use_overlay_scrollers_ = true;
  }

  // Adds a ScrollView with the given |contents_view| and does layout.
  ScrollView* AddScrollViewWithContents(View* contents,
                                        bool commit_layers = true) {
#if defined(OS_MACOSX)
    scroller_style_.reset(
        new ui::test::ScopedPreferredScrollerStyle(use_overlay_scrollers_));
#endif

    const gfx::Rect default_bounds(50, 50, kDefaultWidth, kDefaultHeight);
    widget_ = CreateTopLevelFramelessPlatformWidget();

    ScrollView* scroll_view = new ScrollView();
    scroll_view->SetContents(contents);

    widget_->SetBounds(default_bounds);
    widget_->Show();

    widget_->SetContentsView(scroll_view);
    scroll_view->Layout();

    widget_->GetCompositor()->AddObserver(this);

    // Ensure the Compositor has committed layer changes before attempting to
    // use them for impl-side scrolling. Note that simply RunUntilIdle() works
    // when tests are run in isolation, but compositor scheduling can interact
    // between test runs in the general case.
    if (commit_layers)
      WaitForCommit();
    return scroll_view;
  }

  // Adds a ScrollView with a contents view of the given |size| and does layout.
  ScrollView* AddScrollViewWithContentSize(const gfx::Size& contents_size,
                                           bool commit_layers = true) {
    View* contents = new View;
    contents->SetSize(contents_size);
    return AddScrollViewWithContents(contents, commit_layers);
  }

  // Wait for a commit to be observed on the compositor.
  void WaitForCommit() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, quit_closure_, TestTimeouts::action_timeout());
    run_loop.Run();
    EXPECT_TRUE(quit_closure_.is_null()) << "Timed out waiting for a commit.";
  }

  void TestClickAt(const gfx::Point& location) {
    ui::MouseEvent press(TestLeftMouseAt(location, ui::ET_MOUSE_PRESSED));
    ui::MouseEvent release(TestLeftMouseAt(location, ui::ET_MOUSE_RELEASED));
    widget_->OnMouseEvent(&press);
    widget_->OnMouseEvent(&release);
  }

  // testing::Test:
  void TearDown() override {
    widget_->GetCompositor()->RemoveObserver(this);
    if (widget_)
      widget_->CloseNow();
    WidgetTest::TearDown();
  }

 private:
  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {
    quit_closure_.Run();
    quit_closure_.Reset();
  }
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override {}
  void OnCompositingEnded(ui::Compositor* compositor) override {}
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override {}
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {}

  Widget* widget_ = nullptr;

  // Disable scrollbar hiding (i.e. disable overlay scrollbars) by default.
  bool use_overlay_scrollers_ = false;

  base::Closure quit_closure_;

#if defined(OS_MACOSX)
  std::unique_ptr<ui::test::ScopedPreferredScrollerStyle> scroller_style_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WidgetScrollViewTest);
};

const int WidgetScrollViewTest::kDefaultHeight;
const int WidgetScrollViewTest::kDefaultWidth;

// Verifies the viewport is sized to fit the available space.
TEST_F(ScrollViewTest, ViewportSizedToFit) {
  View* contents = InstallContents();
  scroll_view_.Layout();
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the viewport and content is sized to fit the available space for
// bounded scroll view.
TEST_F(ScrollViewTest, BoundedViewportSizedToFit) {
  View* contents = InstallContents();
  scroll_view_.ClipHeightTo(100, 200);
  scroll_view_.SetBorder(CreateSolidBorder(2, 0));
  scroll_view_.Layout();
  EXPECT_EQ("2,2 96x96", contents->parent()->bounds().ToString());

  // Make sure the width of |contents| is set properly not to overflow the
  // viewport.
  EXPECT_EQ(96, contents->width());
}

// Verifies the scrollbars are added as necessary.
// If on Mac, test the non-overlay scrollbars.
TEST_F(ScrollViewTest, ScrollBars) {
  View* contents = InstallContents();

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view_.Layout();
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth(),
            contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  CheckScrollbarVisibility(scroll_view_, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_, HORIZONTAL, false);
  EXPECT_TRUE(!scroll_view_.horizontal_scroll_bar() ||
              !scroll_view_.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view_.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.vertical_scroll_bar()->visible());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view_.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarHeight(),
            contents->parent()->height());
  CheckScrollbarVisibility(scroll_view_, VERTICAL, false);
  CheckScrollbarVisibility(scroll_view_, HORIZONTAL, true);

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view_.Layout();
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth(),
            contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarHeight(),
            contents->parent()->height());
  CheckScrollbarVisibility(scroll_view_, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_, HORIZONTAL, true);

  // Add a border, test vertical scrollbar.
  const int kTopPadding = 1;
  const int kLeftPadding = 2;
  const int kBottomPadding = 3;
  const int kRightPadding = 4;
  scroll_view_.SetBorder(CreateEmptyBorder(kTopPadding, kLeftPadding,
                                           kBottomPadding, kRightPadding));
  contents->SetBounds(0, 0, 50, 400);
  scroll_view_.Layout();
  EXPECT_EQ(
      100 - scroll_view_.GetScrollBarWidth() - kLeftPadding - kRightPadding,
      contents->parent()->width());
  EXPECT_EQ(100 - kTopPadding - kBottomPadding, contents->parent()->height());
  EXPECT_TRUE(!scroll_view_.horizontal_scroll_bar() ||
              !scroll_view_.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view_.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.vertical_scroll_bar()->visible());
  gfx::Rect bounds = scroll_view_.vertical_scroll_bar()->bounds();
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth() - kRightPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(kTopPadding, bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());

  // Horizontal with border.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view_.Layout();
  EXPECT_EQ(100 - kLeftPadding - kRightPadding, contents->parent()->width());
  EXPECT_EQ(
      100 - scroll_view_.GetScrollBarHeight() - kTopPadding - kBottomPadding,
      contents->parent()->height());
  ASSERT_TRUE(scroll_view_.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.horizontal_scroll_bar()->visible());
  EXPECT_TRUE(!scroll_view_.vertical_scroll_bar() ||
              !scroll_view_.vertical_scroll_bar()->visible());
  bounds = scroll_view_.horizontal_scroll_bar()->bounds();
  EXPECT_EQ(kLeftPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(100 - kBottomPadding - scroll_view_.GetScrollBarHeight(),
            bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());

  // Both horizontal and vertical with border.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view_.Layout();
  EXPECT_EQ(
      100 - scroll_view_.GetScrollBarWidth() - kLeftPadding - kRightPadding,
      contents->parent()->width());
  EXPECT_EQ(
      100 - scroll_view_.GetScrollBarHeight() - kTopPadding - kBottomPadding,
      contents->parent()->height());
  bounds = scroll_view_.horizontal_scroll_bar()->bounds();
  // Check horiz.
  ASSERT_TRUE(scroll_view_.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.horizontal_scroll_bar()->visible());
  bounds = scroll_view_.horizontal_scroll_bar()->bounds();
  EXPECT_EQ(kLeftPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding - scroll_view_.GetScrollBarWidth(),
            bounds.right());
  EXPECT_EQ(100 - kBottomPadding - scroll_view_.GetScrollBarHeight(),
            bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());
  // Check vert.
  ASSERT_TRUE(scroll_view_.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.vertical_scroll_bar()->visible());
  bounds = scroll_view_.vertical_scroll_bar()->bounds();
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth() - kRightPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(kTopPadding, bounds.y());
  EXPECT_EQ(100 - kBottomPadding - scroll_view_.GetScrollBarHeight(),
            bounds.bottom());
}

// Assertions around adding a header.
TEST_F(ScrollViewTest, Header) {
  CustomView* header = new CustomView;
  scroll_view_.SetHeader(header);
  View* header_parent = header->parent();
  View* contents = InstallContents();

  scroll_view_.Layout();
  // |header|s preferred size is empty, which should result in all space going
  // to contents.
  EXPECT_EQ("0,0 100x0", header->parent()->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());

  // With layered scrolling, ScrollView::Layout() will impose a size on the
  // contents that fills the viewport. Since the test view doesn't have its own
  // Layout, reset it in this case so that adding a header doesn't shift the
  // contents down and require scrollbars.
  if (contents->layer()) {
    EXPECT_EQ("0,0 100x100", contents->bounds().ToString());
    contents->SetBoundsRect(gfx::Rect());
  }
  EXPECT_EQ("0,0 0x0", contents->bounds().ToString());

  // Get the header a height of 20.
  header->SetPreferredSize(gfx::Size(10, 20));
  EXPECT_EQ("0,0 100x20", header->parent()->bounds().ToString());
  EXPECT_EQ("0,20 100x80", contents->parent()->bounds().ToString());
  if (contents->layer()) {
    EXPECT_EQ("0,0 100x80", contents->bounds().ToString());
    contents->SetBoundsRect(gfx::Rect());
  }
  EXPECT_EQ("0,0 0x0", contents->bounds().ToString());

  // Remove the header.
  scroll_view_.SetHeader(NULL);
  // SetHeader(NULL) deletes header.
  header = NULL;
  EXPECT_EQ("0,0 100x0", header_parent->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the scrollbars are added as necessary when a header is present.
TEST_F(ScrollViewTest, ScrollBarsWithHeader) {
  CustomView* header = new CustomView;
  scroll_view_.SetHeader(header);
  View* contents = InstallContents();

  header->SetPreferredSize(gfx::Size(10, 20));

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view_.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth(),
            contents->parent()->width());
  EXPECT_EQ(80, contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth(), header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  EXPECT_TRUE(!scroll_view_.horizontal_scroll_bar() ||
              !scroll_view_.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view_.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.vertical_scroll_bar()->visible());
  // Make sure the vertical scrollbar overlaps the header.
  EXPECT_EQ(header->y(), scroll_view_.vertical_scroll_bar()->y());
  EXPECT_EQ(header->y(), contents->y());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view_.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100, header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view_.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.horizontal_scroll_bar()->visible());
  EXPECT_TRUE(!scroll_view_.vertical_scroll_bar() ||
              !scroll_view_.vertical_scroll_bar()->visible());

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view_.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth(),
            contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth(), header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view_.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view_.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view_.vertical_scroll_bar()->visible());
}

// Verifies the header scrolls horizontally with the content.
TEST_F(ScrollViewTest, HeaderScrollsWithContent) {
  ScrollViewTestApi test_api(&scroll_view_);
  CustomView* contents = new CustomView;
  scroll_view_.SetContents(contents);
  contents->SetPreferredSize(gfx::Size(500, 500));

  CustomView* header = new CustomView;
  scroll_view_.SetHeader(header);
  header->SetPreferredSize(gfx::Size(500, 20));

  scroll_view_.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ("0,0", test_api.IntegralViewOffset().ToString());
  EXPECT_EQ("0,0", header->bounds().origin().ToString());

  // Scroll the horizontal scrollbar.
  ASSERT_TRUE(scroll_view_.horizontal_scroll_bar());
  scroll_view_.ScrollToPosition(test_api.GetBaseScrollBar(HORIZONTAL), 1);
  EXPECT_EQ("-1,0", test_api.IntegralViewOffset().ToString());
  EXPECT_EQ("-1,0", header->bounds().origin().ToString());

  // Scrolling the vertical scrollbar shouldn't effect the header.
  ASSERT_TRUE(scroll_view_.vertical_scroll_bar());
  scroll_view_.ScrollToPosition(test_api.GetBaseScrollBar(VERTICAL), 1);
  EXPECT_EQ("-1,-1", test_api.IntegralViewOffset().ToString());
  EXPECT_EQ("-1,0", header->bounds().origin().ToString());
}

// Verifies ScrollRectToVisible() on the child works.
TEST_F(ScrollViewTest, ScrollRectToVisible) {
  ScrollViewTestApi test_api(&scroll_view_);
  CustomView* contents = new CustomView;
  scroll_view_.SetContents(contents);
  contents->SetPreferredSize(gfx::Size(500, 1000));

  scroll_view_.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  scroll_view_.Layout();
  EXPECT_EQ("0,0", test_api.IntegralViewOffset().ToString());

  // Scroll to y=405 height=10, this should make the y position of the content
  // at (405 + 10) - viewport_height (scroll region bottom aligned).
  contents->ScrollRectToVisible(gfx::Rect(0, 405, 10, 10));
  const int viewport_height = test_api.contents_viewport()->height();

  // Expect there to be a horizontal scrollbar, making the viewport shorter.
  EXPECT_LT(viewport_height, 100);

  gfx::ScrollOffset offset = test_api.CurrentOffset();
  EXPECT_EQ(415 - viewport_height, offset.y());

  // Scroll to the current y-location and 10x10; should do nothing.
  contents->ScrollRectToVisible(gfx::Rect(0, offset.y(), 10, 10));
  EXPECT_EQ(415 - viewport_height, test_api.CurrentOffset().y());
}

// Verifies ClipHeightTo() uses the height of the content when it is between the
// minimum and maximum height values.
TEST_F(ScrollViewTest, ClipHeightToNormalContentHeight) {
  scroll_view_.ClipHeightTo(kMinHeight, kMaxHeight);

  const int kNormalContentHeight = 75;
  scroll_view_.SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kNormalContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroll_view_.GetPreferredSize());

  scroll_view_.SizeToPreferredSize();
  scroll_view_.Layout();

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroll_view_.contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight), scroll_view_.size());
}

// Verifies ClipHeightTo() uses the minimum height when the content is shorter
// than the minimum height value.
TEST_F(ScrollViewTest, ClipHeightToShortContentHeight) {
  scroll_view_.ClipHeightTo(kMinHeight, kMaxHeight);

  const int kShortContentHeight = 10;
  View* contents =
      new views::StaticSizedView(gfx::Size(kWidth, kShortContentHeight));
  scroll_view_.SetContents(contents);

  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view_.GetPreferredSize());

  scroll_view_.SizeToPreferredSize();
  scroll_view_.Layout();

  // Layered scrolling requires the contents to fill the viewport.
  if (contents->layer()) {
    EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view_.contents()->size());
  } else {
    EXPECT_EQ(gfx::Size(kWidth, kShortContentHeight),
              scroll_view_.contents()->size());
  }
  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view_.size());
}

// Verifies ClipHeightTo() uses the maximum height when the content is longer
// thamn the maximum height value.
TEST_F(ScrollViewTest, ClipHeightToTallContentHeight) {
  // Use a scrollbar that is disabled by default, so the width of the content is
  // not affected.
  scroll_view_.SetVerticalScrollBar(new views::OverlayScrollBar(false));

  scroll_view_.ClipHeightTo(kMinHeight, kMaxHeight);

  const int kTallContentHeight = 1000;
  scroll_view_.SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kTallContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view_.GetPreferredSize());

  scroll_view_.SizeToPreferredSize();
  scroll_view_.Layout();

  EXPECT_EQ(gfx::Size(kWidth, kTallContentHeight),
            scroll_view_.contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view_.size());
}

// Verifies that when ClipHeightTo() produces a scrollbar, it reduces the width
// of the inner content of the ScrollView.
TEST_F(ScrollViewTest, ClipHeightToScrollbarUsesWidth) {
  scroll_view_.ClipHeightTo(kMinHeight, kMaxHeight);

  // Create a view that will be much taller than it is wide.
  scroll_view_.SetContents(new views::ProportionallySizedView(1000));

  // Without any width, it will default to 0,0 but be overridden by min height.
  scroll_view_.SizeToPreferredSize();
  EXPECT_EQ(gfx::Size(0, kMinHeight), scroll_view_.GetPreferredSize());

  gfx::Size new_size(kWidth, scroll_view_.GetHeightForWidth(kWidth));
  scroll_view_.SetSize(new_size);
  scroll_view_.Layout();

  int scroll_bar_width = scroll_view_.GetScrollBarWidth();
  int expected_width = kWidth - scroll_bar_width;
  EXPECT_EQ(scroll_view_.contents()->size().width(), expected_width);
  EXPECT_EQ(scroll_view_.contents()->size().height(), 1000 * expected_width);
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view_.size());
}

TEST_F(ScrollViewTest, CornerViewVisibility) {
  View* contents = InstallContents();
  View* corner_view = ScrollViewTestApi(&scroll_view_).corner_view();

  // Corner view should be visible when both scrollbars are visible.
  contents->SetBounds(0, 0, 200, 200);
  scroll_view_.Layout();
  EXPECT_EQ(&scroll_view_, corner_view->parent());
  EXPECT_TRUE(corner_view->visible());

  // Corner view should be aligned to the scrollbars.
  EXPECT_EQ(scroll_view_.vertical_scroll_bar()->x(), corner_view->x());
  EXPECT_EQ(scroll_view_.horizontal_scroll_bar()->y(), corner_view->y());
  EXPECT_EQ(scroll_view_.GetScrollBarWidth(), corner_view->width());
  EXPECT_EQ(scroll_view_.GetScrollBarHeight(), corner_view->height());

  // Corner view should be removed when only the vertical scrollbar is visible.
  contents->SetBounds(0, 0, 50, 200);
  scroll_view_.Layout();
  EXPECT_FALSE(corner_view->parent());

  // ... or when only the horizontal scrollbar is visible.
  contents->SetBounds(0, 0, 200, 50);
  scroll_view_.Layout();
  EXPECT_FALSE(corner_view->parent());

  // ... or when no scrollbar is visible.
  contents->SetBounds(0, 0, 50, 50);
  scroll_view_.Layout();
  EXPECT_FALSE(corner_view->parent());

  // Corner view should reappear when both scrollbars reappear.
  contents->SetBounds(0, 0, 200, 200);
  scroll_view_.Layout();
  EXPECT_EQ(&scroll_view_, corner_view->parent());
  EXPECT_TRUE(corner_view->visible());
}

#if defined(OS_MACOSX)
// Tests the overlay scrollbars on Mac. Ensure that they show up properly and
// do not overlap each other.
TEST_F(ScrollViewTest, CocoaOverlayScrollBars) {
  SetOverlayScrollersEnabled(true);
  View* contents = InstallContents();

  // Size the contents such that vertical scrollbar is needed.
  // Since it is overlaid, the ViewPort size should match the ScrollView.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view_.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view_.GetScrollBarWidth());
  CheckScrollbarVisibility(scroll_view_, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_, HORIZONTAL, false);

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view_.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view_.GetScrollBarHeight());
  CheckScrollbarVisibility(scroll_view_, VERTICAL, false);
  CheckScrollbarVisibility(scroll_view_, HORIZONTAL, true);

  // Both horizontal and vertical scrollbars.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view_.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view_.GetScrollBarWidth());
  EXPECT_EQ(0, scroll_view_.GetScrollBarHeight());
  CheckScrollbarVisibility(scroll_view_, VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_, HORIZONTAL, true);

  // Make sure the horizontal and vertical scrollbars don't overlap each other.
  gfx::Rect vert_bounds = scroll_view_.vertical_scroll_bar()->bounds();
  gfx::Rect horiz_bounds = scroll_view_.horizontal_scroll_bar()->bounds();
  EXPECT_EQ(vert_bounds.x(), horiz_bounds.right());
  EXPECT_EQ(horiz_bounds.y(), vert_bounds.bottom());

  // Switch to the non-overlay style and check that the ViewPort is now sized
  // to be smaller, and ScrollbarWidth and ScrollbarHeight are non-zero.
  SetOverlayScrollersEnabled(false);
  EXPECT_EQ(100 - scroll_view_.GetScrollBarWidth(),
            contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_.GetScrollBarHeight(),
            contents->parent()->height());
  EXPECT_NE(0, scroll_view_.GetScrollBarWidth());
  EXPECT_NE(0, scroll_view_.GetScrollBarHeight());
}

// Test overlay scrollbar behavior when just resting fingers on the trackpad.
TEST_F(WidgetScrollViewTest, ScrollersOnRest) {
  // Allow expectations to distinguish between fade outs and immediate changes.
  ui::ScopedAnimationDurationScaleMode really_animate(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const float kMaxOpacity = 0.8f;  // Constant from cocoa_scroll_bar.mm.

  SetUseOverlayScrollers();

  // Set up with both scrollers.
  ScrollView* scroll_view = AddScrollViewWithContentSize(
      gfx::Size(kDefaultWidth * 5, kDefaultHeight * 5));
  ScrollViewTestApi test_api(scroll_view);
  BaseScrollBar* bar[]{test_api.GetBaseScrollBar(HORIZONTAL),
                       test_api.GetBaseScrollBar(VERTICAL)};
  base::Timer* hide_timer[]{test_api.GetScrollBarHideTimer(HORIZONTAL),
                            test_api.GetScrollBarHideTimer(VERTICAL)};

  EXPECT_EQ(0, bar[HORIZONTAL]->layer()->opacity());
  EXPECT_EQ(0, bar[VERTICAL]->layer()->opacity());

  ui::test::EventGenerator generator(
      GetContext(), scroll_view->GetWidget()->GetNativeWindow());

  generator.GenerateTrackpadRest();
  // Scrollers should be max opacity without an animation.
  EXPECT_EQ(kMaxOpacity, bar[HORIZONTAL]->layer()->opacity());
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->opacity());
  EXPECT_FALSE(hide_timer[HORIZONTAL]->IsRunning());
  EXPECT_FALSE(hide_timer[VERTICAL]->IsRunning());

  generator.CancelTrackpadRest();
  // Scrollers should start fading out, but only after a delay.
  for (ScrollBarOrientation orientation : {HORIZONTAL, VERTICAL}) {
    EXPECT_EQ(kMaxOpacity, bar[orientation]->layer()->GetTargetOpacity());
    EXPECT_TRUE(hide_timer[orientation]->IsRunning());
    // Trigger the timer. Should then be fading out.
    hide_timer[orientation]->user_task().Run();
    hide_timer[orientation]->Stop();
    EXPECT_EQ(0, bar[orientation]->layer()->GetTargetOpacity());
  }

  // Rest again.
  generator.GenerateTrackpadRest();
  EXPECT_EQ(kMaxOpacity, bar[HORIZONTAL]->layer()->GetTargetOpacity());
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->GetTargetOpacity());

  // Scroll vertically.
  const float y_offset = 3;
  const int kSteps = 1;
  const int kNnumFingers = 2;
  generator.ScrollSequence(generator.current_location(), base::TimeDelta(), 0,
                           y_offset, kSteps, kNnumFingers);

  // Horizontal scroller should start fading out immediately.
  EXPECT_EQ(kMaxOpacity, bar[HORIZONTAL]->layer()->opacity());
  EXPECT_EQ(0, bar[HORIZONTAL]->layer()->GetTargetOpacity());
  EXPECT_FALSE(hide_timer[HORIZONTAL]->IsRunning());

  // Vertical should remain visible, but ready to fade out after a delay.
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->opacity());
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->GetTargetOpacity());
  EXPECT_TRUE(hide_timer[VERTICAL]->IsRunning());

  // Scrolling should have occurred.
  EXPECT_EQ(gfx::ScrollOffset(0, y_offset), test_api.CurrentOffset());

  // Then, scrolling horizontally should show the horizontal scroller. The
  // vertical scroller should still be visible, running its hide timer.
  const float x_offset = 5;
  generator.ScrollSequence(generator.current_location(), base::TimeDelta(),
                           x_offset, 0, kSteps, kNnumFingers);
  for (ScrollBarOrientation orientation : {HORIZONTAL, VERTICAL}) {
    EXPECT_EQ(kMaxOpacity, bar[orientation]->layer()->opacity());
    EXPECT_EQ(kMaxOpacity, bar[orientation]->layer()->GetTargetOpacity());
    EXPECT_TRUE(hide_timer[orientation]->IsRunning());
  }

  // Now scrolling has occurred in both directions.
  EXPECT_EQ(gfx::ScrollOffset(x_offset, y_offset), test_api.CurrentOffset());
}

#endif  // OS_MACOSX

// Test that increasing the size of the viewport "below" scrolled content causes
// the content to scroll up so that it still fills the viewport.
TEST_F(ScrollViewTest, ConstrainScrollToBounds) {
  ScrollViewTestApi test_api(&scroll_view_);

  View* contents = InstallContents();
  contents->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  scroll_view_.Layout();

  EXPECT_EQ(gfx::ScrollOffset(), test_api.CurrentOffset());

  // Scroll as far as it goes and query location to discount scroll bars.
  contents->ScrollRectToVisible(gfx::Rect(300, 300, 1, 1));
  const gfx::ScrollOffset fully_scrolled = test_api.CurrentOffset();
  EXPECT_NE(gfx::ScrollOffset(), fully_scrolled);

  // Making the viewport 55 pixels taller should scroll up the same amount.
  scroll_view_.SetBoundsRect(gfx::Rect(0, 0, 100, 155));
  scroll_view_.Layout();
  EXPECT_EQ(fully_scrolled.y() - 55, test_api.CurrentOffset().y());
  EXPECT_EQ(fully_scrolled.x(), test_api.CurrentOffset().x());

  // And 77 pixels wider should scroll left. Also make it short again: the y-
  // offset from the last change should remain.
  scroll_view_.SetBoundsRect(gfx::Rect(0, 0, 177, 100));
  scroll_view_.Layout();
  EXPECT_EQ(fully_scrolled.y() - 55, test_api.CurrentOffset().y());
  EXPECT_EQ(fully_scrolled.x() - 77, test_api.CurrentOffset().x());
}

// Calling Layout on ScrollView should not reset the scroll location.
TEST_F(ScrollViewTest, ContentScrollNotResetOnLayout) {
  ScrollViewTestApi test_api(&scroll_view_);

  CustomView* contents = new CustomView;
  contents->SetPreferredSize(gfx::Size(300, 300));
  scroll_view_.SetContents(contents);
  scroll_view_.ClipHeightTo(0, 150);
  scroll_view_.SizeToPreferredSize();
  // ScrollView preferred width matches that of |contents|, with the height
  // capped at the value we clipped to.
  EXPECT_EQ(gfx::Size(300, 150), scroll_view_.size());

  // Scroll down.
  scroll_view_.ScrollToPosition(test_api.GetBaseScrollBar(VERTICAL), 25);
  EXPECT_EQ(25, test_api.CurrentOffset().y());
  // Call Layout; no change to scroll position.
  scroll_view_.Layout();
  EXPECT_EQ(25, test_api.CurrentOffset().y());
  // Change contents of |contents|, call Layout; still no change to scroll
  // position.
  contents->SetPreferredSize(gfx::Size(300, 500));
  contents->InvalidateLayout();
  scroll_view_.Layout();
  EXPECT_EQ(25, test_api.CurrentOffset().y());

  // Change |contents| to be shorter than the ScrollView's clipped height.
  // This /will/ change the scroll location due to ConstrainScrollToBounds.
  contents->SetPreferredSize(gfx::Size(300, 50));
  scroll_view_.Layout();
  EXPECT_EQ(0, test_api.CurrentOffset().y());
}

// Test scrolling behavior when clicking on the scroll track.
TEST_F(WidgetScrollViewTest, ScrollTrackScrolling) {
  // Set up with a vertical scroller.
  ScrollView* scroll_view =
      AddScrollViewWithContentSize(gfx::Size(10, kDefaultHeight * 5));
  ScrollViewTestApi test_api(scroll_view);
  BaseScrollBar* scroll_bar = test_api.GetBaseScrollBar(VERTICAL);
  View* thumb = test_api.GetScrollBarThumb(VERTICAL);

  // Click in the middle of the track, ensuring it's below the thumb.
  const gfx::Point location = scroll_bar->bounds().CenterPoint();
  EXPECT_GT(location.y(), thumb->bounds().bottom());
  ui::MouseEvent press(TestLeftMouseAt(location, ui::ET_MOUSE_PRESSED));
  ui::MouseEvent release(TestLeftMouseAt(location, ui::ET_MOUSE_RELEASED));

  const base::Timer& timer = test_api.GetScrollBarTimer(VERTICAL);
  EXPECT_FALSE(timer.IsRunning());

  EXPECT_EQ(0, scroll_view->GetVisibleRect().y());
  scroll_bar->OnMouseEvent(&press);

  // Clicking the scroll track should scroll one "page".
  EXPECT_EQ(kDefaultHeight, scroll_view->GetVisibleRect().y());

  // While the mouse is pressed, timer should trigger more scroll events.
  EXPECT_TRUE(timer.IsRunning());

  // Upon release timer should stop (and scroll position should remain).
  scroll_bar->OnMouseEvent(&release);
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_EQ(kDefaultHeight, scroll_view->GetVisibleRect().y());
}

// Test that LocatedEvents are transformed correctly when scrolling.
TEST_F(WidgetScrollViewTest, EventLocation) {
  // Set up with both scrollers.
  CustomView* contents = new CustomView;
  contents->SetPreferredSize(gfx::Size(kDefaultHeight * 5, kDefaultHeight * 5));
  AddScrollViewWithContents(contents);

  const gfx::Point location_in_widget(10, 10);

  // Click without scrolling.
  TestClickAt(location_in_widget);
  EXPECT_EQ(location_in_widget, contents->last_location());

  // Scroll down a page.
  contents->ScrollRectToVisible(
      gfx::Rect(0, kDefaultHeight, 1, kDefaultHeight));
  TestClickAt(location_in_widget);
  EXPECT_EQ(gfx::Point(10, 10 + kDefaultHeight), contents->last_location());

  // Scroll right a page (and back up).
  contents->ScrollRectToVisible(gfx::Rect(kDefaultWidth, 0, kDefaultWidth, 1));
  TestClickAt(location_in_widget);
  EXPECT_EQ(gfx::Point(10 + kDefaultWidth, 10), contents->last_location());

  // Scroll both directions.
  contents->ScrollRectToVisible(
      gfx::Rect(kDefaultWidth, kDefaultHeight, kDefaultWidth, kDefaultHeight));
  TestClickAt(location_in_widget);
  EXPECT_EQ(gfx::Point(10 + kDefaultWidth, 10 + kDefaultHeight),
            contents->last_location());
}

// Test that views scroll offsets are in sync with the layer scroll offsets.
TEST_F(WidgetScrollViewTest, ScrollOffsetUsingLayers) {
  // Set up with a vertical scroller, but don't commit the layer changes yet.
  ScrollView* scroll_view =
      AddScrollViewWithContentSize(gfx::Size(10, kDefaultHeight * 5), false);
  ScrollViewTestApi test_api(scroll_view);

  EXPECT_EQ(gfx::ScrollOffset(0, 0), test_api.CurrentOffset());

  // UI code may request a scroll before layer changes are committed.
  gfx::Rect offset(0, kDefaultHeight * 2, 1, kDefaultHeight);
  scroll_view->contents()->ScrollRectToVisible(offset);
  EXPECT_EQ(gfx::ScrollOffset(0, offset.y()), test_api.CurrentOffset());

  // The following only makes sense when layered scrolling is enabled.
  View* container = scroll_view->contents();
#if defined(OS_MACOSX)
  // Sanity check: Mac should always scroll with layers.
  EXPECT_TRUE(container->layer());
#endif
  if (!container->layer())
    return;

  // Container and viewport should have layers.
  EXPECT_TRUE(container->layer());
  EXPECT_TRUE(test_api.contents_viewport()->layer());

  // In a Widget, so there should be a compositor.
  ui::Compositor* compositor = container->layer()->GetCompositor();
  EXPECT_TRUE(compositor);

  // But setting on the impl side should fail since the layer isn't committed.
  int layer_id = container->layer()->cc_layer_for_testing()->id();
  EXPECT_FALSE(compositor->ScrollLayerTo(layer_id, gfx::ScrollOffset(0, 0)));
  EXPECT_EQ(gfx::ScrollOffset(0, offset.y()), test_api.CurrentOffset());

  WaitForCommit();
  EXPECT_EQ(gfx::ScrollOffset(0, offset.y()), test_api.CurrentOffset());

  // Upon commit, the impl side should report the same value too.
  gfx::ScrollOffset impl_offset;
  EXPECT_TRUE(compositor->GetScrollOffsetForLayer(layer_id, &impl_offset));
  EXPECT_EQ(gfx::ScrollOffset(0, offset.y()), impl_offset);

  // Now impl-side scrolling should work, and also update the ScrollView.
  offset.set_y(kDefaultHeight * 3);
  EXPECT_TRUE(
      compositor->ScrollLayerTo(layer_id, gfx::ScrollOffset(0, offset.y())));
  EXPECT_EQ(gfx::ScrollOffset(0, offset.y()), test_api.CurrentOffset());

  // Scroll via ScrollView API. Should be reflected on the impl side.
  offset.set_y(kDefaultHeight * 4);
  scroll_view->contents()->ScrollRectToVisible(offset);
  EXPECT_EQ(gfx::ScrollOffset(0, offset.y()), test_api.CurrentOffset());

  EXPECT_TRUE(compositor->GetScrollOffsetForLayer(layer_id, &impl_offset));
  EXPECT_EQ(gfx::ScrollOffset(0, offset.y()), impl_offset);
}

}  // namespace views
