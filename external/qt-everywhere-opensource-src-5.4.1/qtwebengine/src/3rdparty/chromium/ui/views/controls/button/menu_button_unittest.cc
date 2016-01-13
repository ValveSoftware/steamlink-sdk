// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/test/views_test_base.h"

using base::ASCIIToUTF16;

namespace views {

class MenuButtonTest : public ViewsTestBase {
 public:
  MenuButtonTest() : widget_(NULL), button_(NULL) {}
  virtual ~MenuButtonTest() {}

  virtual void TearDown() OVERRIDE {
    if (widget_ && !widget_->IsClosed())
      widget_->Close();

    ViewsTestBase::TearDown();
  }

  Widget* widget() { return widget_; }
  MenuButton* button() { return button_; }

 protected:
  // Creates a MenuButton with a ButtonListener. In this case, the MenuButton
  // acts like a regular button.
  void CreateMenuButtonWithButtonListener(ButtonListener* button_listener) {
    CreateWidget();

    const base::string16 label(ASCIIToUTF16("button"));
    button_ = new MenuButton(button_listener, label, NULL, false);
    button_->SetBoundsRect(gfx::Rect(0, 0, 200, 20));
    widget_->SetContentsView(button_);

    widget_->Show();
  }

  // Creates a MenuButton with a MenuButtonListener. In this case, when the
  // MenuButton is pushed, it notifies the MenuButtonListener to open a
  // drop-down menu.
  void CreateMenuButtonWithMenuButtonListener(
      MenuButtonListener* menu_button_listener) {
    CreateWidget();

    const base::string16 label(ASCIIToUTF16("button"));
    button_ = new MenuButton(NULL, label, menu_button_listener, false);
    button_->SetBoundsRect(gfx::Rect(0, 0, 200, 20));
    widget_->SetContentsView(button_);

    widget_->Show();
  }

 private:
  void CreateWidget() {
    DCHECK(!widget_);

    widget_ = new Widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);
  }

  Widget* widget_;
  MenuButton* button_;
};

class TestButtonListener : public ButtonListener {
 public:
  TestButtonListener()
      : last_sender_(NULL),
        last_sender_state_(Button::STATE_NORMAL),
        last_event_type_(ui::ET_UNKNOWN) {}
  virtual ~TestButtonListener() {}

  virtual void ButtonPressed(Button* sender, const ui::Event& event) OVERRIDE {
    last_sender_ = sender;
    CustomButton* custom_button = CustomButton::AsCustomButton(sender);
    DCHECK(custom_button);
    last_sender_state_ = custom_button->state();
    last_event_type_ = event.type();
  }

  Button* last_sender() { return last_sender_; }
  Button::ButtonState last_sender_state() { return last_sender_state_; }
  ui::EventType last_event_type() { return last_event_type_; }

 private:
  Button* last_sender_;
  Button::ButtonState last_sender_state_;
  ui::EventType last_event_type_;

  DISALLOW_COPY_AND_ASSIGN(TestButtonListener);
};

class TestMenuButtonListener : public MenuButtonListener {
 public:
  TestMenuButtonListener() {}
  virtual ~TestMenuButtonListener() {}

  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& /*point*/) OVERRIDE {
    last_source_ = source;
    CustomButton* custom_button = CustomButton::AsCustomButton(source);
    DCHECK(custom_button);
    last_source_state_ = custom_button->state();
  }

  View* last_source() { return last_source_; }
  Button::ButtonState last_source_state() { return last_source_state_; }

 private:
  View* last_source_;
  Button::ButtonState last_source_state_;
};

// Tests if the listener is notified correctly, when a mouse click happens on a
// MenuButton that has a regular ButtonListener.
TEST_F(MenuButtonTest, ActivateNonDropDownOnMouseClick) {
  scoped_ptr<TestButtonListener> button_listener(new TestButtonListener);
  CreateMenuButtonWithButtonListener(button_listener.get());

  aura::test::EventGenerator generator(
      widget()->GetNativeView()->GetRootWindow());

  generator.set_current_location(gfx::Point(10, 10));
  generator.ClickLeftButton();

  // Check that MenuButton has notified the listener on mouse-released event,
  // while it was in hovered state.
  EXPECT_EQ(button(), button_listener->last_sender());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, button_listener->last_event_type());
  EXPECT_EQ(Button::STATE_HOVERED, button_listener->last_sender_state());
}

// Tests if the listener is notified correctly when a gesture tap happens on a
// MenuButton that has a regular ButtonListener.
TEST_F(MenuButtonTest, ActivateNonDropDownOnGestureTap) {
  scoped_ptr<TestButtonListener> button_listener(new TestButtonListener);
  CreateMenuButtonWithButtonListener(button_listener.get());

  aura::test::EventGenerator generator(
      widget()->GetNativeView()->GetRootWindow());

  generator.GestureTapAt(gfx::Point(10, 10));

  // Check that MenuButton has notified the listener on gesture tap event, while
  // it was in hovered state.
  EXPECT_EQ(button(), button_listener->last_sender());
  EXPECT_EQ(ui::ET_GESTURE_TAP, button_listener->last_event_type());
  EXPECT_EQ(Button::STATE_HOVERED, button_listener->last_sender_state());
}

// Tests if the listener is notified correctly when a mouse click happens on a
// MenuButton that has a MenuButtonListener.
TEST_F(MenuButtonTest, ActivateDropDownOnMouseClick) {
  scoped_ptr<TestMenuButtonListener> menu_button_listener(
      new TestMenuButtonListener);
  CreateMenuButtonWithMenuButtonListener(menu_button_listener.get());

  aura::test::EventGenerator generator(
      widget()->GetNativeView()->GetRootWindow());

  generator.set_current_location(gfx::Point(10, 10));
  generator.ClickLeftButton();

  // Check that MenuButton has notified the listener, while it was in pressed
  // state.
  EXPECT_EQ(button(), menu_button_listener->last_source());
  EXPECT_EQ(Button::STATE_PRESSED, menu_button_listener->last_source_state());
}

// Tests if the listener is notified correctly when a gesture tap happens on a
// MenuButton that has a MenuButtonListener.
TEST_F(MenuButtonTest, ActivateDropDownOnGestureTap) {
  scoped_ptr<TestMenuButtonListener> menu_button_listener(
      new TestMenuButtonListener);
  CreateMenuButtonWithMenuButtonListener(menu_button_listener.get());

  aura::test::EventGenerator generator(
      widget()->GetNativeView()->GetRootWindow());

  generator.GestureTapAt(gfx::Point(10, 10));

  // Check that MenuButton has notified the listener, while it was in pressed
  // state.
  EXPECT_EQ(button(), menu_button_listener->last_source());
  EXPECT_EQ(Button::STATE_PRESSED, menu_button_listener->last_source_state());
}

}  // namespace views
