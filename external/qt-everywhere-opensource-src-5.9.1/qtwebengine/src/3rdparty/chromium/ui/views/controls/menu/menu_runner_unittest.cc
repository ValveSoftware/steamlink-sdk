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
  // TODO: test uses GetContext(), which is not applicable to aura-mus.
  // http://crbug.com/663809.
  if (IsAuraMusClient())
    return;

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
  // TODO: test uses GetContext(), which is not applicable to aura-mus.
  // http://crbug.com/663809.
  if (IsAuraMusClient())
    return;

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
  // TODO: test uses GetContext(), which is not applicable to aura-mus.
  // http://crbug.com/663809.
  if (IsAuraMusClient())
    return;

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

// Test harness that includes a parent Widget and View invoking the menu.
class MenuRunnerWidgetTest : public MenuRunnerTest {
 public:
  MenuRunnerWidgetTest() {}

  Widget* widget() { return widget_; }
  EventCountView* event_count_view() { return event_count_view_; }

  std::unique_ptr<ui::test::EventGenerator> EventGeneratorForWidget(
      Widget* widget) {
    return base::MakeUnique<ui::test::EventGenerator>(
        IsMus() || IsAuraMusClient() ? widget->GetNativeWindow() : GetContext(),
        widget->GetNativeWindow());
  }

  void AddMenuLauncherEventHandler(Widget* widget) {
    consumer_ =
        base::MakeUnique<MenuLauncherEventHandler>(menu_runner(), widget);
    event_count_view_->AddPostTargetHandler(consumer_.get());
  }

  // ViewsTestBase:
  void SetUp() override {
    MenuRunnerTest::SetUp();
    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    widget_->Init(params);
    widget_->Show();
    widget_->SetSize(gfx::Size(300, 300));

    event_count_view_ = new EventCountView();
    event_count_view_->SetBounds(0, 0, 300, 300);
    widget_->GetRootView()->AddChildView(event_count_view_);

    InitMenuRunner(MenuRunner::ASYNC);
  }

  void TearDown() override {
    widget_->CloseNow();
    MenuRunnerTest::TearDown();
  }

 private:
  Widget* widget_ = nullptr;
  EventCountView* event_count_view_ = nullptr;
  std::unique_ptr<MenuLauncherEventHandler> consumer_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerWidgetTest);
};

// Tests that when a mouse press launches a menu, that the target widget does
// not take explicit capture, nor closes the menu.
TEST_F(MenuRunnerWidgetTest, WidgetDoesntTakeCapture) {
  AddMenuLauncherEventHandler(owner());

  EXPECT_EQ(nullptr, internal::NativeWidgetPrivate::GetGlobalCapture(
                         widget()->GetNativeView()));
  auto generator(EventGeneratorForWidget(widget()));
  // Implicit capture should not be held by |widget|.
  generator->PressLeftButton();
  EXPECT_EQ(1, event_count_view()->GetEventCount(ui::ET_MOUSE_PRESSED));
  EXPECT_NE(widget()->GetNativeView(),
            internal::NativeWidgetPrivate::GetGlobalCapture(
                widget()->GetNativeView()));

  // The menu should still be open.
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_TRUE(menu_runner()->IsRunning());
  EXPECT_EQ(0, delegate->on_menu_closed_called());
}

// Tests that after showing a menu on mouse press, that the subsequent mouse
// will be delivered to the correct view, and not to the one that showed the
// menu.
//
// The original bug is reproducible only when showing the menu on mouse press,
// as RootView::OnMouseReleased() doesn't have the same behavior.
TEST_F(MenuRunnerWidgetTest, ClearsMouseHandlerOnRun) {
  AddMenuLauncherEventHandler(widget());

  // Create a second view that's supposed to get the second mouse press.
  EventCountView* second_event_count_view = new EventCountView();
  widget()->GetRootView()->AddChildView(second_event_count_view);

  widget()->SetBounds(gfx::Rect(0, 0, 200, 100));
  event_count_view()->SetBounds(0, 0, 100, 100);
  second_event_count_view->SetBounds(100, 0, 100, 100);

  // Click on the first view to show the menu.
  auto generator(EventGeneratorForWidget(widget()));
  generator->MoveMouseTo(event_count_view()->bounds().CenterPoint());
  generator->PressLeftButton();

  // Pretend we dismissed the menu using normal means, as it doesn't matter.
  EXPECT_TRUE(menu_runner()->IsRunning());
  menu_runner()->Cancel();

  // EventGenerator won't allow us to re-send the left button press without
  // releasing it first. We can't send the release event using the same
  // generator as it would be handled by the RootView in the main Widget.
  // In actual application the RootView doesn't see the release event.
  generator.reset();
  generator = EventGeneratorForWidget(widget());

  generator->MoveMouseTo(second_event_count_view->bounds().CenterPoint());
  generator->PressLeftButton();
  EXPECT_EQ(1, second_event_count_view->GetEventCount(ui::ET_MOUSE_PRESSED));
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
