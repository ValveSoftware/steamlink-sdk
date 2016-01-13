// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/custom_button.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/layout.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace views {

namespace {

class TestCustomButton : public CustomButton {
 public:
  explicit TestCustomButton(ButtonListener* listener)
      : CustomButton(listener) {
  }

  virtual ~TestCustomButton() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCustomButton);
};

void PerformGesture(CustomButton* button, ui::EventType event_type) {
  ui::GestureEventDetails gesture_details(event_type, 0, 0);
  base::TimeDelta time_stamp = base::TimeDelta::FromMicroseconds(0);
  ui::GestureEvent gesture_event(gesture_details.type(), 0, 0, 0, time_stamp,
                                 gesture_details, 1);
  button->OnGestureEvent(&gesture_event);
}

}  // namespace

typedef ViewsTestBase CustomButtonTest;

// Tests that hover state changes correctly when visiblity/enableness changes.
TEST_F(CustomButtonTest, HoverStateOnVisibilityChange) {
  // Create a widget so that the CustomButton can query the hover state
  // correctly.
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget->Init(params);
  widget->Show();

  aura::test::TestCursorClient cursor_client(
      widget->GetNativeView()->GetRootWindow());

  // Position the widget in a way so that it is under the cursor.
  gfx::Point cursor = gfx::Screen::GetScreenFor(
      widget->GetNativeView())->GetCursorScreenPoint();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  widget_bounds.set_origin(cursor);
  widget->SetBounds(widget_bounds);

  TestCustomButton* button = new TestCustomButton(NULL);
  widget->SetContentsView(button);

  gfx::Point center(10, 10);
  button->OnMousePressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED, center, center,
                                        ui::EF_LEFT_MOUSE_BUTTON,
                                        ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_PRESSED, button->state());

  button->OnMouseReleased(ui::MouseEvent(ui::ET_MOUSE_RELEASED, center, center,
                                         ui::EF_LEFT_MOUSE_BUTTON,
                                         ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_HOVERED, button->state());

  button->SetEnabled(false);
  EXPECT_EQ(CustomButton::STATE_DISABLED, button->state());

  button->SetEnabled(true);
  EXPECT_EQ(CustomButton::STATE_HOVERED, button->state());

  button->SetVisible(false);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button->state());

  button->SetVisible(true);
  EXPECT_EQ(CustomButton::STATE_HOVERED, button->state());

  // In Aura views, no new hover effects are invoked if mouse events
  // are disabled.
  cursor_client.DisableMouseEvents();

  button->SetEnabled(false);
  EXPECT_EQ(CustomButton::STATE_DISABLED, button->state());

  button->SetEnabled(true);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button->state());

  button->SetVisible(false);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button->state());

  button->SetVisible(true);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button->state());
}

// Tests that gesture events correctly change the button state.
TEST_F(CustomButtonTest, GestureEventsSetState) {
  // Create a widget so that the CustomButton can query the hover state
  // correctly.
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget->Init(params);
  widget->Show();

  aura::test::TestCursorClient cursor_client(
      widget->GetNativeView()->GetRootWindow());

  TestCustomButton* button = new TestCustomButton(NULL);
  widget->SetContentsView(button);

  EXPECT_EQ(CustomButton::STATE_NORMAL, button->state());

  PerformGesture(button, ui::ET_GESTURE_TAP_DOWN);
  EXPECT_EQ(CustomButton::STATE_PRESSED, button->state());

  PerformGesture(button, ui::ET_GESTURE_SHOW_PRESS);
  EXPECT_EQ(CustomButton::STATE_PRESSED, button->state());

  PerformGesture(button, ui::ET_GESTURE_TAP_CANCEL);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button->state());
}

// Make sure all subclasses of CustomButton are correctly recognized
// as CustomButton.
TEST_F(CustomButtonTest, AsCustomButton) {
  base::string16 text;

  TextButton text_button(NULL, text);
  EXPECT_TRUE(CustomButton::AsCustomButton(&text_button));

  ImageButton image_button(NULL);
  EXPECT_TRUE(CustomButton::AsCustomButton(&image_button));

  Checkbox checkbox(text);
  EXPECT_TRUE(CustomButton::AsCustomButton(&checkbox));

  RadioButton radio_button(text, 0);
  EXPECT_TRUE(CustomButton::AsCustomButton(&radio_button));

  MenuButton menu_button(NULL, text, NULL, false);
  EXPECT_TRUE(CustomButton::AsCustomButton(&menu_button));

  Label label;
  EXPECT_FALSE(CustomButton::AsCustomButton(&label));

  Link link(text);
  EXPECT_FALSE(CustomButton::AsCustomButton(&link));

  Textfield textfield;
  EXPECT_FALSE(CustomButton::AsCustomButton(&textfield));
}

}  // namespace views
