// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view.h"

#include "ui/events/event_targeter.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

typedef ViewsTestBase RootViewTest;

class DeleteOnKeyEventView : public View {
 public:
  explicit DeleteOnKeyEventView(bool* set_on_key) : set_on_key_(set_on_key) {}
  virtual ~DeleteOnKeyEventView() {}

  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE {
    *set_on_key_ = true;
    delete this;
    return true;
  }

 private:
  // Set to true in OnKeyPressed().
  bool* set_on_key_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnKeyEventView);
};

// Verifies deleting a View in OnKeyPressed() doesn't crash and that the
// target is marked as destroyed in the returned EventDispatchDetails.
TEST_F(RootViewTest, DeleteViewDuringKeyEventDispatch) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  bool got_key_event = false;

  View* content = new View;
  widget.SetContentsView(content);

  View* child = new DeleteOnKeyEventView(&got_key_event);
  content->AddChildView(child);

  // Give focus to |child| so that it will be the target of the key event.
  child->SetFocusable(true);
  child->RequestFocus();

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0, false);
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&key_event);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(got_key_event);
}

// Tracks whether a context menu is shown.
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController()
      : show_context_menu_calls_(0),
        menu_source_view_(NULL),
        menu_source_type_(ui::MENU_SOURCE_NONE) {
  }
  virtual ~TestContextMenuController() {}

  int show_context_menu_calls() const { return show_context_menu_calls_; }
  View* menu_source_view() const { return menu_source_view_; }
  ui::MenuSourceType menu_source_type() const { return menu_source_type_; }

  void Reset() {
    show_context_menu_calls_ = 0;
    menu_source_view_ = NULL;
    menu_source_type_ = ui::MENU_SOURCE_NONE;
  }

  // ContextMenuController:
  virtual void ShowContextMenuForView(
      View* source,
      const gfx::Point& point,
      ui::MenuSourceType source_type) OVERRIDE {
    show_context_menu_calls_++;
    menu_source_view_ = source;
    menu_source_type_ = source_type;
  }

 private:
  int show_context_menu_calls_;
  View* menu_source_view_;
  ui::MenuSourceType menu_source_type_;

  DISALLOW_COPY_AND_ASSIGN(TestContextMenuController);
};

// Tests that context menus are shown for certain key events (Shift+F10
// and VKEY_APPS) by the pre-target handler installed on RootView.
TEST_F(RootViewTest, ContextMenuFromKeyEvent) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());

  TestContextMenuController controller;
  View* focused_view = new View;
  focused_view->set_context_menu_controller(&controller);
  widget.SetContentsView(focused_view);
  focused_view->SetFocusable(true);
  focused_view->RequestFocus();

  // No context menu should be shown for a keypress of 'A'.
  ui::KeyEvent nomenu_key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, true);
  ui::EventDispatchDetails details =
      root_view->OnEventFromSource(&nomenu_key_event);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  EXPECT_EQ(NULL, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_NONE, controller.menu_source_type());
  controller.Reset();

  // A context menu should be shown for a keypress of Shift+F10.
  ui::KeyEvent menu_key_event(
      ui::ET_KEY_PRESSED, ui::VKEY_F10, ui::EF_SHIFT_DOWN, false);
  details = root_view->OnEventFromSource(&menu_key_event);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();

  // A context menu should be shown for a keypress of VKEY_APPS.
  ui::KeyEvent menu_key_event2(ui::ET_KEY_PRESSED, ui::VKEY_APPS, 0, false);
  details = root_view->OnEventFromSource(&menu_key_event2);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();
}

// View which handles all gesture events.
class GestureHandlingView : public View {
 public:
  GestureHandlingView() {
  }

  virtual ~GestureHandlingView() {
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    event->SetHandled();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureHandlingView);
};

// Tests that context menus are shown for long press by the post-target handler
// installed on the RootView only if the event is targetted at a view which can
// show a context menu.
TEST_F(RootViewTest, ContextMenuFromLongPress) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(100,100);
  widget.Init(init_params);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());

  // Create a view capable of showing the context menu with two children one of
  // which handles all gesture events (e.g. a button).
  TestContextMenuController controller;
  View* parent_view = new View;
  parent_view->set_context_menu_controller(&controller);
  widget.SetContentsView(parent_view);

  View* gesture_handling_child_view = new GestureHandlingView;
  gesture_handling_child_view->SetBoundsRect(gfx::Rect(10,10));
  parent_view->AddChildView(gesture_handling_child_view);

  View* other_child_view = new View;
  other_child_view->SetBoundsRect(gfx::Rect(20, 0, 10,10));
  parent_view->AddChildView(other_child_view);

  // |parent_view| should not show a context menu as a result of a long press on
  // |gesture_handling_child_view|.
  ui::GestureEvent begin1(ui::ET_GESTURE_BEGIN, 5, 5, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_BEGIN, 0, 0), 1);
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&begin1);

  ui::GestureEvent long_press1(ui::ET_GESTURE_LONG_PRESS, 5, 5, 0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS, 0, 0), 1);
  details = root_view->OnEventFromSource(&long_press1);

  ui::GestureEvent end1(ui::ET_GESTURE_END, 5, 5, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_END, 0, 0), 1);
  details = root_view->OnEventFromSource(&end1);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());

  // |parent_view| should show a context menu as a result of a long press on
  // |other_child_view|.
  ui::GestureEvent begin2(ui::ET_GESTURE_BEGIN, 25, 5, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_BEGIN, 0, 0), 1);
  details = root_view->OnEventFromSource(&begin2);

  ui::GestureEvent long_press2(ui::ET_GESTURE_LONG_PRESS, 25, 5, 0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS, 0, 0), 1);
  details = root_view->OnEventFromSource(&long_press2);

  ui::GestureEvent end2(ui::ET_GESTURE_END, 25, 5, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_END, 0, 0), 1);
  details = root_view->OnEventFromSource(&end2);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
}

}  // namespace test
}  // namespace views
