// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_window_targeter.h"

#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class WidgetOwnsNativeBubble : public BubbleDelegateView {
 public:
  WidgetOwnsNativeBubble(View* content, BubbleBorder::Arrow arrow)
      : BubbleDelegateView(content, arrow) {
  }

  virtual ~WidgetOwnsNativeBubble() {}

 private:
  // BubbleDelegateView:
  virtual void OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                        Widget* widget) const OVERRIDE {
    params->ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  }

  DISALLOW_COPY_AND_ASSIGN(WidgetOwnsNativeBubble);
};

}  // namespace

class BubbleWindowTargeterTest : public ViewsTestBase {
 public:
  BubbleWindowTargeterTest()
      : bubble_delegate_(NULL) {
  }
  virtual ~BubbleWindowTargeterTest() {}

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();
    CreateAnchorWidget();
    CreateBubbleWidget();

    anchor_widget()->Show();
    bubble_widget()->Show();
  }

  virtual void TearDown() OVERRIDE {
    bubble_delegate_ = NULL;
    bubble_widget_.reset();
    anchor_.reset();
    ViewsTestBase::TearDown();
  }

  Widget* anchor_widget() { return anchor_.get(); }
  Widget* bubble_widget() { return bubble_widget_.get(); }
  BubbleDelegateView* bubble_delegate() { return bubble_delegate_; }

 private:
  void CreateAnchorWidget() {
    anchor_.reset(new Widget());
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_->Init(params);
  }

  void CreateBubbleWidget() {
    bubble_delegate_ = new WidgetOwnsNativeBubble(
        anchor_->GetContentsView(), BubbleBorder::NONE);
    bubble_delegate_->set_color(SK_ColorGREEN);
    bubble_widget_.reset(BubbleDelegateView::CreateBubble(bubble_delegate_));
  }

  scoped_ptr<Widget> anchor_;
  scoped_ptr<Widget> bubble_widget_;
  BubbleDelegateView* bubble_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWindowTargeterTest);
};

TEST_F(BubbleWindowTargeterTest, HitTest) {
  ui::EventTarget* root = bubble_widget()->GetNativeWindow()->GetRootWindow();
  ui::EventTargeter* targeter = root->GetEventTargeter();
  aura::Window* bubble_window = bubble_widget()->GetNativeWindow();
  gfx::Rect bubble_bounds = bubble_window->GetBoundsInRootWindow();

  {
    bubble_delegate()->set_margins(gfx::Insets());
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, bubble_bounds.origin(),
                         bubble_bounds.origin(), ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(bubble_window, targeter->FindTargetForEvent(root, &move1));
  }
  {
    bubble_delegate()->set_margins(gfx::Insets(20, 20, 20, 20));
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, bubble_bounds.origin(),
                         bubble_bounds.origin(), ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(bubble_window, targeter->FindTargetForEvent(root, &move1));
  }

  bubble_window->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
      new BubbleWindowTargeter(bubble_delegate())));
  {
    bubble_delegate()->set_margins(gfx::Insets(20, 20, 20, 20));
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, bubble_bounds.origin(),
                         bubble_bounds.origin(), ui::EF_NONE, ui::EF_NONE);
    EXPECT_NE(bubble_window, targeter->FindTargetForEvent(root, &move1));
  }
}

}  // namespace views
