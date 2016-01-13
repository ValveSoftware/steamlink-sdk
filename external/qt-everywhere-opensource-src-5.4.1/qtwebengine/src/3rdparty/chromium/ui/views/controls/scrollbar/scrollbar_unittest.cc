// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/native_scroll_bar.h"
#include "ui/views/controls/scrollbar/native_scroll_bar_views.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

// The Scrollbar controller. This is the widget that should do the real
// scrolling of contents.
class TestScrollBarController : public views::ScrollBarController {
 public:
  virtual ~TestScrollBarController() {}

  virtual void ScrollToPosition(views::ScrollBar* source,
                                int position) OVERRIDE {
    last_source = source;
    last_position = position;
  }

  virtual int GetScrollIncrement(views::ScrollBar* source,
                                 bool is_page,
                                 bool is_positive) OVERRIDE {
    last_source = source;
    last_is_page = is_page;
    last_is_positive = is_positive;

    if (is_page)
      return 20;
    return 10;
  }

  // We save the last values in order to assert the corectness of the scroll
  // operation.
  views::ScrollBar* last_source;
  bool last_is_positive;
  bool last_is_page;
  int last_position;
};

}  // namespace

namespace views {

class NativeScrollBarTest : public ViewsTestBase {
 public:
  NativeScrollBarTest() : widget_(NULL), scrollbar_(NULL) {}

  virtual void SetUp() {
    ViewsTestBase::SetUp();
    controller_.reset(new TestScrollBarController());

    ASSERT_FALSE(scrollbar_);
    native_scrollbar_ = new NativeScrollBar(true);
    native_scrollbar_->SetBounds(0, 0, 100, 100);
    native_scrollbar_->set_controller(controller_.get());

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(native_scrollbar_);

    scrollbar_ =
        static_cast<NativeScrollBarViews*>(native_scrollbar_->native_wrapper_);
    scrollbar_->SetBounds(0, 0, 100, 100);
    scrollbar_->Update(100, 200, 0);

    track_size_ = scrollbar_->GetTrackBounds().width();
  }

  virtual void TearDown() {
    widget_->Close();
    ViewsTestBase::TearDown();
  }

 protected:
  Widget* widget_;

  // This is the native scrollbar the Views one wraps around.
  NativeScrollBar* native_scrollbar_;

  // This is the Views scrollbar.
  BaseScrollBar* scrollbar_;

  // Keep track of the size of the track. This is how we can tell when we
  // scroll to the middle.
  int track_size_;

  scoped_ptr<TestScrollBarController> controller_;
};

// TODO(dnicoara) Can't run the test on Windows since the scrollbar |Part|
// isn't handled in NativeTheme.
#if defined(OS_WIN)
#define MAYBE_Scrolling DISABLED_Scrolling
#define MAYBE_ScrollBarFitsToBottom DISABLED_ScrollBarFitsToBottom
#else
#define MAYBE_Scrolling Scrolling
#define MAYBE_ScrollBarFitsToBottom ScrollBarFitsToBottom
#endif

TEST_F(NativeScrollBarTest, MAYBE_Scrolling) {
  EXPECT_EQ(scrollbar_->GetPosition(), 0);
  EXPECT_EQ(scrollbar_->GetMaxPosition(), 100);
  EXPECT_EQ(scrollbar_->GetMinPosition(), 0);

  // Scroll to middle.
  scrollbar_->ScrollToThumbPosition(track_size_ / 4, false);
  EXPECT_EQ(controller_->last_position, 50);
  EXPECT_EQ(controller_->last_source, native_scrollbar_);

  // Scroll to the end.
  scrollbar_->ScrollToThumbPosition(track_size_ / 2, false);
  EXPECT_EQ(controller_->last_position, 100);

  // Overscroll. Last position should be the maximum position.
  scrollbar_->ScrollToThumbPosition(track_size_, false);
  EXPECT_EQ(controller_->last_position, 100);

  // Underscroll. Last position should be the minimum position.
  scrollbar_->ScrollToThumbPosition(-10, false);
  EXPECT_EQ(controller_->last_position, 0);

  // Test the different fixed scrolling amounts. Generally used by buttons,
  // or click on track.
  scrollbar_->ScrollToThumbPosition(0, false);
  scrollbar_->ScrollByAmount(BaseScrollBar::SCROLL_NEXT_LINE);
  EXPECT_EQ(controller_->last_position, 10);

  scrollbar_->ScrollByAmount(BaseScrollBar::SCROLL_PREV_LINE);
  EXPECT_EQ(controller_->last_position, 0);

  scrollbar_->ScrollByAmount(BaseScrollBar::SCROLL_NEXT_PAGE);
  EXPECT_EQ(controller_->last_position, 20);

  scrollbar_->ScrollByAmount(BaseScrollBar::SCROLL_PREV_PAGE);
  EXPECT_EQ(controller_->last_position, 0);
}

TEST_F(NativeScrollBarTest, MAYBE_ScrollBarFitsToBottom) {
  scrollbar_->Update(100, 199, 0);
  EXPECT_EQ(0, scrollbar_->GetPosition());
  EXPECT_EQ(99, scrollbar_->GetMaxPosition());
  EXPECT_EQ(0, scrollbar_->GetMinPosition());

  scrollbar_->Update(100, 199, 99);
  EXPECT_EQ(
      scrollbar_->GetTrackBounds().width() - scrollbar_->GetThumbSizeForTest(),
      scrollbar_->GetPosition());
}

TEST_F(NativeScrollBarTest, ScrollToEndAfterShrinkAndExpand) {
  // Scroll to the end of the content.
  scrollbar_->Update(100, 101, 0);
  EXPECT_TRUE(scrollbar_->ScrollByContentsOffset(-1));
  // Shrink and then re-exapnd the content.
  scrollbar_->Update(100, 100, 0);
  scrollbar_->Update(100, 101, 0);
  // Ensure the scrollbar allows scrolling to the end.
  EXPECT_TRUE(scrollbar_->ScrollByContentsOffset(-1));
}

}  // namespace views
