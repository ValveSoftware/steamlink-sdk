// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner_impl.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

class MenuRunnerTest : public ViewsTestBase {
 public:
  MenuRunnerTest();
  ~MenuRunnerTest() override;

  // Initializes a MenuRunner with |run_types|. It takes ownership of
  // |menu_item_view_|.
  void InitMenuRunner(int32_t run_types);

  MenuItemView* menu_item_view() { return menu_item_view_; }
  TestMenuDelegate* menu_delegate() { return menu_delegate_.get(); }
  MenuRunner* menu_runner() { return menu_runner_.get(); }
  Widget* owner() { return owner_.get(); }

  // ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // Owned by MenuRunner.
  MenuItemView* menu_item_view_;

  std::unique_ptr<TestMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<Widget> owner_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerTest);
};

MenuRunnerTest::MenuRunnerTest() {}

MenuRunnerTest::~MenuRunnerTest() {}

void MenuRunnerTest::InitMenuRunner(int32_t run_types) {
  menu_runner_.reset(new MenuRunner(menu_item_view_, run_types));
}

void MenuRunnerTest::SetUp() {
  ViewsTestBase::SetUp();
  menu_delegate_.reset(new TestMenuDelegate);
  menu_item_view_ = new MenuItemView(menu_delegate_.get());
  menu_item_view_->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
  menu_item_view_->AppendMenuItemWithLabel(2,
                                           base::WideToUTF16(L"\x062f\x0648"));

  owner_.reset(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  owner_->Init(params);
  owner_->Show();
}

void MenuRunnerTest::TearDown() {
  owner_->CloseNow();
  ViewsTestBase::TearDown();
}

// Tests that MenuRunner is still running after the call to RunMenuAt when
// initialized with MenuRunner::ASYNC, and that MenuDelegate is notified upon
// the closing of the menu.
TEST_F(MenuRunnerTest, AsynchronousRun) {
  InitMenuRunner(MenuRunner::ASYNC);
  MenuRunner* runner = menu_runner();
  MenuRunner::RunResult result = runner->RunMenuAt(
      owner(), nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_TRUE(runner->IsRunning());

  runner->Cancel();
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, delegate->on_menu_closed_run_result());
}

// Tests that when a menu is run asynchronously, key events are handled properly
// by testing that Escape key closes the menu.
TEST_F(MenuRunnerTest, AsynchronousKeyEventHandling) {
  InitMenuRunner(MenuRunner::ASYNC);
  MenuRunner* runner = menu_runner();
  MenuRunner::RunResult result = runner->RunMenuAt(
      owner(), nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  generator.PressKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, delegate->on_menu_closed_run_result());
}

// Tests that a key press on a US keyboard layout activates the correct menu
// item.
TEST_F(MenuRunnerTest, LatinMnemonic) {
  InitMenuRunner(MenuRunner::ASYNC);
  MenuRunner* runner = menu_runner();
  MenuRunner::RunResult result = runner->RunMenuAt(
      owner(), nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  generator.PressKey(ui::VKEY_O, 0);
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->execute_command_id());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, delegate->on_menu_closed_run_result());
}

// Tests that a key press on a non-US keyboard layout activates the correct menu
// item.
TEST_F(MenuRunnerTest, NonLatinMnemonic) {
  InitMenuRunner(MenuRunner::ASYNC);
  MenuRunner* runner = menu_runner();
  MenuRunner::RunResult result = runner->RunMenuAt(
      owner(), nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  ui::KeyEvent key_press(0x062f, ui::VKEY_N, 0);
  generator.Dispatch(&key_press);
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(2, delegate->execute_command_id());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, delegate->on_menu_closed_run_result());
}

// Tests that attempting to nest a menu within a drag-and-drop menu does not
// cause a crash. Instead the drag and drop action should be canceled, and the
// new menu should be openned.
TEST_F(MenuRunnerTest, NestingDuringDrag) {
  InitMenuRunner(MenuRunner::FOR_DROP);
  MenuRunner* runner = menu_runner();
  MenuRunner::RunResult result = runner->RunMenuAt(
      owner(), nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_TRUE(runner->IsRunning());

  std::unique_ptr<TestMenuDelegate> nested_delegate(new TestMenuDelegate);
  MenuItemView* nested_menu = new MenuItemView(nested_delegate.get());
  std::unique_ptr<MenuRunner> nested_runner(
      new MenuRunner(nested_menu, MenuRunner::IS_NESTED | MenuRunner::ASYNC));
  result = nested_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                                    MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_TRUE(nested_runner->IsRunning());
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, delegate->on_menu_closed_run_result());
}

namespace {

// An EventHandler that launches a menu in response to a mouse press.
class MenuLauncherEventHandler : public ui::EventHandler {
 public:
  MenuLauncherEventHandler(MenuRunner* runner, Widget* owner)
      : runner_(runner), owner_(owner) {}
  ~MenuLauncherEventHandler() override {}

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      runner_->RunMenuAt(owner_, nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                         ui::MENU_SOURCE_NONE);
      event->SetHandled();
    }
  }

  MenuRunner* runner_;
  Widget* owner_;

  DISALLOW_COPY_AND_ASSIGN(MenuLauncherEventHandler);
};

}  // namespace

// Tests that when a mouse press launches a menu, that the target widget does
// not take explicit capture, nor closes the menu.
TEST_F(MenuRunnerTest, WidgetDoesntTakeCapture) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  widget->Init(params);
  widget->Show();
  widget->SetSize(gfx::Size(300, 300));

  EventCountView* event_count_view = new EventCountView();
  event_count_view->SetBounds(0, 0, 300, 300);
  widget->GetRootView()->AddChildView(event_count_view);

  InitMenuRunner(MenuRunner::ASYNC);
  MenuRunner* runner = menu_runner();

  MenuLauncherEventHandler consumer(runner, owner());
  event_count_view->AddPostTargetHandler(&consumer);
  EXPECT_EQ(nullptr, internal::NativeWidgetPrivate::GetGlobalCapture(
                         widget->GetNativeView()));
  std::unique_ptr<ui::test::EventGenerator> generator(
      new ui::test::EventGenerator(
          IsMus() ? widget->GetNativeWindow() : GetContext(),
          widget->GetNativeWindow()));
  // Implicit capture should not be held by |widget|.
  generator->PressLeftButton();
  EXPECT_EQ(1, event_count_view->GetEventCount(ui::ET_MOUSE_PRESSED));
  EXPECT_NE(
      widget->GetNativeView(),
      internal::NativeWidgetPrivate::GetGlobalCapture(widget->GetNativeView()));

  // The menu should still be open.
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_TRUE(runner->IsRunning());
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  widget->CloseNow();
}

typedef MenuRunnerTest MenuRunnerImplTest;

// Tests that when nested menu runners are destroyed out of order, that
// MenuController is not accessed after it has been destroyed. This should not
// crash on ASAN bots.
TEST_F(MenuRunnerImplTest, NestedMenuRunnersDestroyedOutOfOrder) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(menu_item_view());
  EXPECT_EQ(MenuRunner::NORMAL_EXIT,
            menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                                   MENU_ANCHOR_TOPLEFT, MenuRunner::ASYNC));

  std::unique_ptr<TestMenuDelegate> menu_delegate2(new TestMenuDelegate);
  MenuItemView* menu_item_view2 = new MenuItemView(menu_delegate2.get());
  menu_item_view2->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));

  internal::MenuRunnerImpl* menu_runner2 =
      new internal::MenuRunnerImpl(menu_item_view2);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT,
            menu_runner2->RunMenuAt(owner(), nullptr, gfx::Rect(),
                                    MENU_ANCHOR_TOPLEFT,
                                    MenuRunner::ASYNC | MenuRunner::IS_NESTED));

  // Hide the controller so we can test out of order destruction.
  MenuControllerTestApi menu_controller;
  menu_controller.Hide();

  // This destroyed MenuController
  menu_runner->OnMenuClosed(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
                            nullptr, 0);

  // This should not access the destroyed MenuController
  menu_runner2->Release();
  menu_runner->Release();
}

}  // namespace test
}  // namespace views
