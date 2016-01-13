// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/hit_test.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

class TestBubbleDelegateView : public BubbleDelegateView {
 public:
  TestBubbleDelegateView(View* anchor_view)
      : BubbleDelegateView(anchor_view, BubbleBorder::TOP_LEFT),
        view_(new View()) {
    view_->SetFocusable(true);
    AddChildView(view_);
  }
  virtual ~TestBubbleDelegateView() {}

  void SetAnchorRectForTest(gfx::Rect rect) {
    SetAnchorRect(rect);
  }

  void SetAnchorViewForTest(View* view) {
    SetAnchorView(view);
  }

  // BubbleDelegateView overrides:
  virtual View* GetInitiallyFocusedView() OVERRIDE { return view_; }
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(200, 200);
  }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleDelegateView);
};

class BubbleDelegateTest : public ViewsTestBase {
 public:
  BubbleDelegateTest() {}
  virtual ~BubbleDelegateTest() {}

  // Creates a test widget that owns its native widget.
  Widget* CreateTestWidget() {
    Widget* widget = new Widget();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget->Init(params);
    return widget;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleDelegateTest);
};

}  // namespace

TEST_F(BubbleDelegateTest, CreateDelegate) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  bubble_delegate->set_color(SK_ColorGREEN);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble_widget->Show();

  BubbleBorder* border = bubble_delegate->GetBubbleFrameView()->bubble_border();
  EXPECT_EQ(bubble_delegate->arrow(), border->arrow());
  EXPECT_EQ(bubble_delegate->color(), border->background_color());

  EXPECT_FALSE(bubble_observer.widget_closed());
  bubble_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDelegateTest, CloseAnchorWidget) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  // Preventing close on deactivate should not prevent closing with the anchor.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  bubble_widget->Show();
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  scoped_ptr<Widget> smoke_and_mirrors_widget(CreateTestWidget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Ensure that closing the anchor widget also closes the bubble itself.
  anchor_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

// This test checks that the bubble delegate is capable to handle an early
// destruction of the used anchor view. (Animations and delayed closure of the
// bubble will call upon the anchor view to get its location).
TEST_F(BubbleDelegateTest, CloseAnchorViewTest) {
  // Create an anchor widget and add a view to be used as an anchor view.
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  scoped_ptr<View> anchor_view(new View());
  anchor_widget->GetContentsView()->AddChildView(anchor_view.get());
  TestBubbleDelegateView* bubble_delegate = new TestBubbleDelegateView(
      anchor_view.get());
  // Prevent flakes by avoiding closing on activation changes.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);

  // Check that the anchor view is correct and set up an anchor view rect.
  // Make sure that this rect will get ignored (as long as the anchor view is
  // attached).
  EXPECT_EQ(anchor_view, bubble_delegate->GetAnchorView());
  const gfx::Rect set_anchor_rect = gfx::Rect(10, 10, 100, 100);
  bubble_delegate->SetAnchorRectForTest(set_anchor_rect);
  const gfx::Rect view_rect = bubble_delegate->GetAnchorRect();
  EXPECT_NE(view_rect.ToString(), set_anchor_rect.ToString());

  // Create the bubble.
  bubble_widget->Show();
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());

  // Remove now the anchor view and make sure that the original found rect
  // is still kept, so that the bubble does not jump when the view gets deleted.
  anchor_widget->GetContentsView()->RemoveChildView(anchor_view.get());
  anchor_view.reset();
  EXPECT_EQ(NULL, bubble_delegate->GetAnchorView());
  EXPECT_EQ(view_rect.ToString(), bubble_delegate->GetAnchorRect().ToString());
}

// Testing that a move of the anchor view will lead to new bubble locations.
TEST_F(BubbleDelegateTest, TestAnchorRectMovesWithViewTest) {
  // Create an anchor widget and add a view to be used as anchor view.
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDelegateView* bubble_delegate = new TestBubbleDelegateView(
      anchor_widget->GetContentsView());
  BubbleDelegateView::CreateBubble(bubble_delegate);

  anchor_widget->GetContentsView()->SetBounds(10, 10, 100, 100);
  const gfx::Rect view_rect = bubble_delegate->GetAnchorRect();

  anchor_widget->GetContentsView()->SetBounds(20, 10, 100, 100);
  const gfx::Rect view_rect_2 = bubble_delegate->GetAnchorRect();
  EXPECT_NE(view_rect.ToString(), view_rect_2.ToString());
}

TEST_F(BubbleDelegateTest, ResetAnchorWidget) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);

  // Make sure the bubble widget is parented to a widget other than the anchor
  // widget so that closing the anchor widget does not close the bubble widget.
  scoped_ptr<Widget> parent_widget(CreateTestWidget());
  bubble_delegate->set_parent_window(parent_widget->GetNativeView());
  // Preventing close on deactivate should not prevent closing with the parent.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Showing and hiding the bubble widget should have no effect on its anchor.
  bubble_widget->Show();
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  bubble_widget->Hide();
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());

  // Ensure that closing the anchor widget clears the bubble's reference to that
  // anchor widget, but the bubble itself does not close.
  anchor_widget->CloseNow();
  EXPECT_NE(anchor_widget, bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  scoped_ptr<Widget> smoke_and_mirrors_widget(CreateTestWidget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Ensure that closing the parent widget also closes the bubble itself.
  parent_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDelegateTest, InitiallyFocusedView) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate->GetInitiallyFocusedView(),
            bubble_widget->GetFocusManager()->GetFocusedView());
  bubble_widget->CloseNow();
}

TEST_F(BubbleDelegateTest, NonClientHitTest) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDelegateView* bubble_delegate =
      new TestBubbleDelegateView(anchor_widget->GetContentsView());
  BubbleDelegateView::CreateBubble(bubble_delegate);
  BubbleFrameView* frame = bubble_delegate->GetBubbleFrameView();
  const int border = frame->bubble_border()->GetBorderThickness();

  struct {
    const int point;
    const int hit;
  } cases[] = {
    { border,      HTNOWHERE },
    { border + 50, HTCLIENT  },
    { 1000,        HTNOWHERE },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    gfx::Point point(cases[i].point, cases[i].point);
    EXPECT_EQ(cases[i].hit, frame->NonClientHitTest(point))
        << " with border: " << border << ", at point " << cases[i].point;
  }
}

TEST_F(BubbleDelegateTest, VisibleWhenAnchorWidgetBoundsChanged) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  bubble_widget->Show();
  EXPECT_TRUE(bubble_widget->IsVisible());
  anchor_widget->SetBounds(gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(bubble_widget->IsVisible());
}

}  // namespace views
