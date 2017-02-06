// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/custom_button.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/animation/test/test_ink_drop_host.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#endif

namespace views {

using test::InkDropHostViewTestApi;
using test::TestInkDrop;

namespace {

// No-op test double of a ContextMenuController.
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController() {}
  ~TestContextMenuController() override {}

  // ContextMenuController:
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContextMenuController);
};

class TestCustomButton : public CustomButton, public ButtonListener {
 public:
  explicit TestCustomButton()
      : CustomButton(this) {
  }

  ~TestCustomButton() override {}

  void ButtonPressed(Button* sender, const ui::Event& event) override {
    pressed_ = true;
  }

  void OnClickCanceled(const ui::Event& event) override {
    canceled_ = true;
  }

  bool pressed() { return pressed_; }
  bool canceled() { return canceled_; }

  void Reset() {
    pressed_ = false;
    canceled_ = false;
  }

  // Raised visibility of OnFocus() to public
  void OnFocus() override { CustomButton::OnFocus(); }

 private:
  bool pressed_ = false;
  bool canceled_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCustomButton);
};

}  // namespace

class CustomButtonTest : public ViewsTestBase {
 public:
  CustomButtonTest() {}
  ~CustomButtonTest() override {}

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the CustomButton can query the hover state
    // correctly.
    widget_.reset(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(params);
    widget_->Show();

    button_ = new TestCustomButton();
    widget_->SetContentsView(button_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void CreateButtonWithInkDrop(std::unique_ptr<InkDrop> ink_drop) {
    delete button_;
    button_ = new TestCustomButton();
    InkDropHostViewTestApi(button_).SetInkDrop(std::move(ink_drop));
    widget_->SetContentsView(button_);
  }

 protected:
  Widget* widget() { return widget_.get(); }
  TestCustomButton* button() { return button_; }
  void SetDraggedView(View* dragged_view) {
    widget_->dragged_view_ = dragged_view;
  }

  void SimulateKeyEvent(ui::KeyEvent* event) { widget()->OnKeyEvent(event); }

 private:
  std::unique_ptr<Widget> widget_;
  TestCustomButton* button_;

  DISALLOW_COPY_AND_ASSIGN(CustomButtonTest);
};

// Tests that hover state changes correctly when visiblity/enableness changes.
TEST_F(CustomButtonTest, HoverStateOnVisibilityChange) {
  ui::test::EventGenerator generator(widget()->GetNativeWindow());

  generator.PressLeftButton();
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());

  generator.ReleaseLeftButton();
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());

  button()->SetEnabled(false);
  EXPECT_EQ(CustomButton::STATE_DISABLED, button()->state());

  button()->SetEnabled(true);
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());

  button()->SetVisible(false);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

  button()->SetVisible(true);
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());

#if defined(USE_AURA)
  {
    // If another widget has capture, the button should ignore mouse position
    // and not enter hovered state.
    Widget second_widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(700, 700, 10, 10);
    second_widget.Init(params);
    second_widget.Show();
    second_widget.GetNativeWindow()->SetCapture();

    button()->SetEnabled(false);
    EXPECT_EQ(CustomButton::STATE_DISABLED, button()->state());

    button()->SetEnabled(true);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

    button()->SetVisible(false);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

    button()->SetVisible(true);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());
  }
#endif

// Disabling cursor events occurs for touch events and the Ash magnifier. There
// is no touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)
  aura::test::TestCursorClient cursor_client(
      widget()->GetNativeView()->GetRootWindow());

  // Mus doesn't support disabling mouse events. https://crbug.com/618321
  if (!IsMus()) {
    // In Aura views, no new hover effects are invoked if mouse events
    // are disabled.
    cursor_client.DisableMouseEvents();

    button()->SetEnabled(false);
    EXPECT_EQ(CustomButton::STATE_DISABLED, button()->state());

    button()->SetEnabled(true);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

    button()->SetVisible(false);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

    button()->SetVisible(true);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());
  }
#endif  // !defined(OS_MACOSX) || defined(USE_AURA)
}

// Tests the different types of NotifyActions.
TEST_F(CustomButtonTest, NotifyAction) {
  gfx::Point center(10, 10);

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());
  EXPECT_FALSE(button()->pressed());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());
  EXPECT_TRUE(button()->pressed());

  // Set the notify action to its listener on mouse press.
  button()->Reset();
  button()->set_notify_action(CustomButton::NOTIFY_ON_PRESS);
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());
  EXPECT_TRUE(button()->pressed());

  // The button should no longer notify on mouse release.
  button()->Reset();
  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());
  EXPECT_FALSE(button()->pressed());
}

// Tests that OnClickCanceled gets called when NotifyClick is not expected
// anymore.
TEST_F(CustomButtonTest, NotifyActionNoClick) {
  gfx::Point center(10, 10);

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(button()->canceled());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_TRUE(button()->canceled());

  // Set the notify action to its listener on mouse press.
  button()->Reset();
  button()->set_notify_action(CustomButton::NOTIFY_ON_PRESS);
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  // OnClickCanceled is only sent on mouse release.
  EXPECT_FALSE(button()->canceled());

  // The button should no longer notify on mouse release.
  button()->Reset();
  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(button()->canceled());
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)

namespace {

void PerformGesture(CustomButton* button, ui::EventType event_type) {
  ui::GestureEventDetails gesture_details(event_type);
  ui::GestureEvent gesture_event(0, 0, 0, base::TimeTicks(), gesture_details);
  button->OnGestureEvent(&gesture_event);
}

}  // namespace

// Tests that gesture events correctly change the button state.
TEST_F(CustomButtonTest, GestureEventsSetState) {
  aura::test::TestCursorClient cursor_client(
      widget()->GetNativeView()->GetRootWindow());

  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_TAP_DOWN);
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_SHOW_PRESS);
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_TAP_CANCEL);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());
}

#endif  // !defined(OS_MACOSX) || defined(USE_AURA)

// Ensure subclasses of CustomButton are correctly recognized as CustomButton.
TEST_F(CustomButtonTest, AsCustomButton) {
  base::string16 text;

  LabelButton label_button(NULL, text);
  EXPECT_TRUE(CustomButton::AsCustomButton(&label_button));

  ImageButton image_button(NULL);
  EXPECT_TRUE(CustomButton::AsCustomButton(&image_button));

  Checkbox checkbox(text);
  EXPECT_TRUE(CustomButton::AsCustomButton(&checkbox));

  RadioButton radio_button(text, 0);
  EXPECT_TRUE(CustomButton::AsCustomButton(&radio_button));

  MenuButton menu_button(text, NULL, false);
  EXPECT_TRUE(CustomButton::AsCustomButton(&menu_button));

  Label label;
  EXPECT_FALSE(CustomButton::AsCustomButton(&label));

  Link link(text);
  EXPECT_FALSE(CustomButton::AsCustomButton(&link));

  Textfield textfield;
  EXPECT_FALSE(CustomButton::AsCustomButton(&textfield));
}

// Tests that pressing a button shows the ink drop and releasing the button
// does not hide the ink drop.
// Note: Ink drop is not hidden upon release because CustomButton descendants
// may enter a different ink drop state.
TEST_F(CustomButtonTest, ButtonClickTogglesInkDrop) {
  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));

  ui::test::EventGenerator generator(widget()->GetNativeWindow());
  generator.set_current_location(gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  generator.ReleaseLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

// Tests that pressing a button shows and releasing capture hides ink drop.
// Releasing capture should also reset PRESSED button state to NORMAL.
TEST_F(CustomButtonTest, CaptureLossHidesInkDrop) {
  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));

  ui::test::EventGenerator generator(widget()->GetNativeWindow());
  generator.set_current_location(gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  EXPECT_EQ(Button::ButtonState::STATE_PRESSED, button()->state());
  SetDraggedView(button());
  widget()->SetCapture(button());
  widget()->ReleaseCapture();
  SetDraggedView(nullptr);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
  EXPECT_EQ(ui::MaterialDesignController::IsModeMaterial()
                ? Button::ButtonState::STATE_NORMAL
                : Button::ButtonState::STATE_PRESSED,
            button()->state());
}

TEST_F(CustomButtonTest, HideInkDropWhenShowingContextMenu) {
  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));
  TestContextMenuController context_menu_controller;
  button()->set_context_menu_controller(&context_menu_controller);
  button()->set_hide_ink_drop_when_showing_context_menu(true);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_FALSE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
}

TEST_F(CustomButtonTest, DontHideInkDropWhenShowingContextMenu) {
  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));
  TestContextMenuController context_menu_controller;
  button()->set_context_menu_controller(&context_menu_controller);
  button()->set_hide_ink_drop_when_showing_context_menu(false);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

TEST_F(CustomButtonTest, HideInkDropOnBlur) {
  gfx::Point center(10, 10);

  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));

  button()->OnFocus();

  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnBlur();
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(button()->pressed());
}

TEST_F(CustomButtonTest, InkDropAfterTryingToShowContextMenu) {
  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));
  button()->set_context_menu_controller(nullptr);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

TEST_F(CustomButtonTest, InkDropShowHideOnMouseDragged) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));

  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_FALSE(button()->pressed());
}

TEST_F(CustomButtonTest, InkDropStaysHiddenWhileDragging) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = new TestInkDrop();
  CreateButtonWithInkDrop(base::WrapUnique(ink_drop));

  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  SetDraggedView(button());
  widget()->SetCapture(button());
  widget()->ReleaseCapture();

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  SetDraggedView(nullptr);
}

// Todo(karandeepb): On Mac, a button should get clicked on a Space key press
// (and not release). Modify this test after fixing crbug.com/607429.
// Test that Space Key behaves correctly on a focused button.
TEST_F(CustomButtonTest, ClickOnSpace) {
  // Give focus to the button.
  button()->SetFocusForPlatform();
  button()->RequestFocus();
  EXPECT_EQ(button(), widget()->GetFocusManager()->GetFocusedView());

  ui::KeyEvent space_press(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE);
  SimulateKeyEvent(&space_press);
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());
  EXPECT_FALSE(button()->pressed());

  ui::KeyEvent space_release(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE);
  SimulateKeyEvent(&space_release);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());
  EXPECT_TRUE(button()->pressed());
}

}  // namespace views
