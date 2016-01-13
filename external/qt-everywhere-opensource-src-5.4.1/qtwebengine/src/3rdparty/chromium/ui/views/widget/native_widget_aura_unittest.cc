// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/screen.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/default_activation_client.h"

namespace views {
namespace {

NativeWidgetAura* Init(aura::Window* parent, Widget* widget) {
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent;
  widget->Init(params);
  return static_cast<NativeWidgetAura*>(widget->native_widget());
}

class NativeWidgetAuraTest : public aura::test::AuraTestBase {
 public:
  NativeWidgetAuraTest() {}
  virtual ~NativeWidgetAuraTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    AuraTestBase::SetUp();
    new wm::DefaultActivationClient(root_window());
    host()->SetBounds(gfx::Rect(640, 480));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetAuraTest);
};

TEST_F(NativeWidgetAuraTest, CenterWindowLargeParent) {
  // Make a parent window larger than the host represented by
  // WindowEventDispatcher.
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 1024, 800));
  scoped_ptr<Widget> widget(new Widget());
  NativeWidgetAura* window = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect( (640 - 100) / 2,
                       (480 - 100) / 2,
                       100, 100),
            window->GetNativeWindow()->bounds());
  widget->CloseNow();
}

TEST_F(NativeWidgetAuraTest, CenterWindowSmallParent) {
  // Make a parent window smaller than the host represented by
  // WindowEventDispatcher.
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 480, 320));
  scoped_ptr<Widget> widget(new Widget());
  NativeWidgetAura* window = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect( (480 - 100) / 2,
                       (320 - 100) / 2,
                       100, 100),
            window->GetNativeWindow()->bounds());
  widget->CloseNow();
}

// Verifies CenterWindow() constrains to parent size.
TEST_F(NativeWidgetAuraTest, CenterWindowSmallParentNotAtOrigin) {
  // Make a parent window smaller than the host represented by
  // WindowEventDispatcher and offset it slightly from the origin.
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(20, 40, 480, 320));
  scoped_ptr<Widget> widget(new Widget());
  NativeWidgetAura* window = Init(parent.get(), widget.get());
  window->CenterWindow(gfx::Size(500, 600));

  // |window| should be no bigger than |parent|.
  EXPECT_EQ("20,40 480x320", window->GetNativeWindow()->bounds().ToString());
  widget->CloseNow();
}

class TestLayoutManagerBase : public aura::LayoutManager {
 public:
  TestLayoutManagerBase() {}
  virtual ~TestLayoutManagerBase() {}

  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLayoutManagerBase);
};

// Used by ShowMaximizedDoesntBounceAround. See it for details.
class MaximizeLayoutManager : public TestLayoutManagerBase {
 public:
  MaximizeLayoutManager() {}
  virtual ~MaximizeLayoutManager() {}

 private:
  // aura::LayoutManager:
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    // This simulates what happens when adding a maximized window.
    SetChildBoundsDirect(child, gfx::Rect(0, 0, 300, 300));
  }

  DISALLOW_COPY_AND_ASSIGN(MaximizeLayoutManager);
};

// This simulates BrowserView, which creates a custom RootView so that
// OnNativeWidgetSizeChanged that is invoked during Init matters.
class TestWidget : public views::Widget {
 public:
  TestWidget() : did_size_change_more_than_once_(false) {
  }

  // Returns true if the size changes to a non-empty size, and then to another
  // size.
  bool did_size_change_more_than_once() const {
    return did_size_change_more_than_once_;
  }

  virtual void OnNativeWidgetSizeChanged(const gfx::Size& new_size) OVERRIDE {
    if (last_size_.IsEmpty())
      last_size_ = new_size;
    else if (!did_size_change_more_than_once_ && new_size != last_size_)
      did_size_change_more_than_once_ = true;
    Widget::OnNativeWidgetSizeChanged(new_size);
  }

 private:
  bool did_size_change_more_than_once_;
  gfx::Size last_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWidget);
};

// Verifies the size of the widget doesn't change more than once during Init if
// the window ends up maximized. This is important as otherwise
// RenderWidgetHostViewAura ends up getting resized during construction, which
// leads to noticable flashes.
TEST_F(NativeWidgetAuraTest, ShowMaximizedDoesntBounceAround) {
  root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
  root_window()->SetLayoutManager(new MaximizeLayoutManager);
  scoped_ptr<TestWidget> widget(new TestWidget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = NULL;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_MAXIMIZED;
  params.bounds = gfx::Rect(10, 10, 100, 200);
  widget->Init(params);
  EXPECT_FALSE(widget->did_size_change_more_than_once());
  widget->CloseNow();
}

class PropertyTestLayoutManager : public TestLayoutManagerBase {
 public:
  PropertyTestLayoutManager() : added_(false) {}
  virtual ~PropertyTestLayoutManager() {}

  bool added() const { return added_; }

 private:
  // aura::LayoutManager:
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    EXPECT_TRUE(child->GetProperty(aura::client::kCanMaximizeKey));
    EXPECT_TRUE(child->GetProperty(aura::client::kCanResizeKey));
    added_ = true;
  }

  bool added_;

  DISALLOW_COPY_AND_ASSIGN(PropertyTestLayoutManager);
};

class PropertyTestWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit PropertyTestWidgetDelegate(Widget* widget) : widget_(widget) {}
  virtual ~PropertyTestWidgetDelegate() {}

 private:
  // views::WidgetDelegate:
  virtual bool CanMaximize() const OVERRIDE {
    return true;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }
  virtual Widget* GetWidget() OVERRIDE {
    return widget_;
  }
  virtual const Widget* GetWidget() const OVERRIDE {
    return widget_;
  }

  Widget* widget_;
  DISALLOW_COPY_AND_ASSIGN(PropertyTestWidgetDelegate);
};

// Verifies that the kCanMaximizeKey/kCanReizeKey have the correct
// value when added to the layout manager.
TEST_F(NativeWidgetAuraTest, TestPropertiesWhenAddedToLayout) {
  root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
  PropertyTestLayoutManager* layout_manager = new PropertyTestLayoutManager();
  root_window()->SetLayoutManager(layout_manager);
  scoped_ptr<TestWidget> widget(new TestWidget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = new PropertyTestWidgetDelegate(widget.get());
  params.parent = NULL;
  params.context = root_window();
  widget->Init(params);
  EXPECT_TRUE(layout_manager->added());
  widget->CloseNow();
}

TEST_F(NativeWidgetAuraTest, GetClientAreaScreenBounds) {
  // Create a widget.
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = root_window();
  params.bounds.SetRect(10, 20, 300, 400);
  scoped_ptr<Widget> widget(new Widget());
  widget->Init(params);

  // For Aura, client area bounds match window bounds.
  gfx::Rect client_bounds = widget->GetClientAreaBoundsInScreen();
  EXPECT_EQ(10, client_bounds.x());
  EXPECT_EQ(20, client_bounds.y());
  EXPECT_EQ(300, client_bounds.width());
  EXPECT_EQ(400, client_bounds.height());
}

// View subclass that tracks whether it has gotten a gesture event.
class GestureTrackingView : public views::View {
 public:
  GestureTrackingView()
      : got_gesture_event_(false),
        consume_gesture_event_(true) {}

  void set_consume_gesture_event(bool value) {
    consume_gesture_event_ = value;
  }

  void clear_got_gesture_event() {
    got_gesture_event_ = false;
  }
  bool got_gesture_event() const {
    return got_gesture_event_;
  }

  // View overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    got_gesture_event_ = true;
    if (consume_gesture_event_)
      event->StopPropagation();
  }

 private:
  // Was OnGestureEvent() invoked?
  bool got_gesture_event_;

  // Dictates what OnGestureEvent() returns.
  bool consume_gesture_event_;

  DISALLOW_COPY_AND_ASSIGN(GestureTrackingView);
};

// Verifies a capture isn't set on touch press and that the view that gets
// the press gets the release.
TEST_F(NativeWidgetAuraTest, DontCaptureOnGesture) {
  // Create two views (both sized the same). |child| is configured not to
  // consume the gesture event.
  GestureTrackingView* view = new GestureTrackingView();
  GestureTrackingView* child = new GestureTrackingView();
  child->set_consume_gesture_event(false);
  view->SetLayoutManager(new FillLayout);
  view->AddChildView(child);
  scoped_ptr<TestWidget> widget(new TestWidget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = root_window();
  params.bounds = gfx::Rect(0, 0, 100, 200);
  widget->Init(params);
  widget->SetContentsView(view);
  widget->Show();

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(41, 51), 1, ui::EventTimeForNow());
  ui::EventDispatchDetails details =
      event_processor()->OnEventFromSource(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  // Both views should get the press.
  EXPECT_TRUE(view->got_gesture_event());
  EXPECT_TRUE(child->got_gesture_event());
  view->clear_got_gesture_event();
  child->clear_got_gesture_event();
  // Touch events should not automatically grab capture.
  EXPECT_FALSE(widget->HasCapture());

  // Release touch. Only |view| should get the release since that it consumed
  // the press.
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(250, 251), 1, ui::EventTimeForNow());
  details = event_processor()->OnEventFromSource(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(view->got_gesture_event());
  EXPECT_FALSE(child->got_gesture_event());
  view->clear_got_gesture_event();

  // Work around for bug in NativeWidgetAura.
  // TODO: fix bug and remove this.
  widget->Close();
}

// Verifies views with layers are targeted for events properly.
TEST_F(NativeWidgetAuraTest, PreferViewLayersToChildWindows) {
  // Create two widgets: |parent| and |child|. |child| is a child of |parent|.
  views::View* parent_root = new views::View;
  scoped_ptr<Widget> parent(new Widget());
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  parent_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_params.context = root_window();
  parent->Init(parent_params);
  parent->SetContentsView(parent_root);
  parent->SetBounds(gfx::Rect(0, 0, 400, 400));
  parent->Show();

  scoped_ptr<Widget> child(new Widget());
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.parent = parent->GetNativeWindow();
  child->Init(child_params);
  child->SetBounds(gfx::Rect(0, 0, 200, 200));
  child->Show();

  // Point is over |child|.
  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(50, 50)));

  // Create a view with a layer and stack it at the bottom (below |child|).
  views::View* view_with_layer = new views::View;
  parent_root->AddChildView(view_with_layer);
  view_with_layer->SetBounds(0, 0, 50, 50);
  view_with_layer->SetPaintToLayer(true);

  // Make sure that |child| still gets the event.
  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(20, 20)));

  // Move |view_with_layer| to the top and make sure it gets the
  // event when the point is within |view_with_layer|'s bounds.
  view_with_layer->layer()->parent()->StackAtTop(
      view_with_layer->layer());
  EXPECT_EQ(parent->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(20, 20)));

  // Point is over |child|, it should get the event.
  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(70, 70)));

  delete view_with_layer;
  view_with_layer = NULL;

  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(20, 20)));

  // Work around for bug in NativeWidgetAura.
  // TODO: fix bug and remove this.
  parent->Close();
}

// Verifies that widget->FlashFrame() sets aura::client::kDrawAttentionKey,
// and activating the window clears it.
TEST_F(NativeWidgetAuraTest, FlashFrame) {
  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.context = root_window();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  aura::Window* window = widget->GetNativeWindow();
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(true);
  EXPECT_TRUE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(false);
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(true);
  EXPECT_TRUE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->Activate();
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
}

TEST_F(NativeWidgetAuraTest, NoCrashOnThemeAfterClose) {
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 480, 320));
  scoped_ptr<Widget> widget(new Widget());
  Init(parent.get(), widget.get());
  widget->Show();
  widget->Close();
  base::MessageLoop::current()->RunUntilIdle();
  widget->GetNativeTheme();  // Shouldn't crash.
}

// Used to track calls to WidgetDelegate::OnWidgetMove().
class MoveTestWidgetDelegate : public WidgetDelegateView {
 public:
  MoveTestWidgetDelegate() : got_move_(false) {}
  virtual ~MoveTestWidgetDelegate() {}

  void ClearGotMove() { got_move_ = false; }
  bool got_move() const { return got_move_; }

  // WidgetDelegate overrides:
  virtual void OnWidgetMove() OVERRIDE { got_move_ = true; }

 private:
  bool got_move_;

  DISALLOW_COPY_AND_ASSIGN(MoveTestWidgetDelegate);
};

// This test simulates what happens when a window is normally maximized. That
// is, it's layer is acquired for animation then the window is maximized.
// Acquiring the layer resets the bounds of the window. This test verifies the
// Widget is still notified correctly of a move in this case.
TEST_F(NativeWidgetAuraTest, OnWidgetMovedInvokedAfterAcquireLayer) {
  // |delegate| deletes itself when the widget is destroyed.
  MoveTestWidgetDelegate* delegate = new MoveTestWidgetDelegate;
  Widget* widget =
      Widget::CreateWindowWithContextAndBounds(delegate,
                                               root_window(),
                                               gfx::Rect(10, 10, 100, 200));
  widget->Show();
  delegate->ClearGotMove();
  // Simulate a maximize with animation.
  delete widget->GetNativeView()->RecreateLayer().release();
  widget->SetBounds(gfx::Rect(0, 0, 500, 500));
  EXPECT_TRUE(delegate->got_move());
  widget->CloseNow();
}

}  // namespace
}  // namespace views
