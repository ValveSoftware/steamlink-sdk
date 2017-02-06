// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/null_event_targeter.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_message_loop.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/views/controls/menu/menu_key_event_handler.h"
#endif

#if defined(USE_X11)
#include <X11/Xlib.h>
#undef Bool
#undef None
#include "ui/events/test/events_test_utils_x11.h"
#endif

namespace views {
namespace test {

namespace {

// Test implementation of MenuControllerDelegate that only reports the values
// called of OnMenuClosed.
class TestMenuControllerDelegate : public internal::MenuControllerDelegate {
 public:
  TestMenuControllerDelegate();
  ~TestMenuControllerDelegate() override {}

  int on_menu_closed_called() { return on_menu_closed_called_; }

  NotifyType on_menu_closed_notify_type() {
    return on_menu_closed_notify_type_;
  }

  MenuItemView* on_menu_closed_menu() { return on_menu_closed_menu_; }

  int on_menu_closed_mouse_event_flags() {
    return on_menu_closed_mouse_event_flags_;
  }

  // On a subsequent call to OnMenuClosed |controller| will be deleted.
  void set_on_menu_closed_callback(const base::Closure& callback) {
    on_menu_closed_callback_ = callback;
  }

  // internal::MenuControllerDelegate:
  void OnMenuClosed(NotifyType type,
                    MenuItemView* menu,
                    int mouse_event_flags) override;
  void SiblingMenuCreated(MenuItemView* menu) override;

 private:
  // Number of times OnMenuClosed has been called.
  int on_menu_closed_called_;

  // The values passed on the last call of OnMenuClosed.
  NotifyType on_menu_closed_notify_type_;
  MenuItemView* on_menu_closed_menu_;
  int on_menu_closed_mouse_event_flags_;

  // Optional callback triggered during OnMenuClosed
  base::Closure on_menu_closed_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuControllerDelegate);
};

TestMenuControllerDelegate::TestMenuControllerDelegate()
    : on_menu_closed_called_(0),
      on_menu_closed_notify_type_(NOTIFY_DELEGATE),
      on_menu_closed_menu_(nullptr),
      on_menu_closed_mouse_event_flags_(0),
      on_menu_closed_callback_() {}

void TestMenuControllerDelegate::OnMenuClosed(NotifyType type,
                                              MenuItemView* menu,
                                              int mouse_event_flags) {
  on_menu_closed_called_++;
  on_menu_closed_notify_type_ = type;
  on_menu_closed_menu_ = menu;
  on_menu_closed_mouse_event_flags_ = mouse_event_flags;
  if (!on_menu_closed_callback_.is_null())
    on_menu_closed_callback_.Run();
}

void TestMenuControllerDelegate::SiblingMenuCreated(MenuItemView* menu) {}

class SubmenuViewShown : public SubmenuView {
 public:
  SubmenuViewShown(MenuItemView* parent) : SubmenuView(parent) {}
  ~SubmenuViewShown() override {}
  bool IsShowing() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubmenuViewShown);
};

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() : outstanding_touches_(0) {}

  void OnTouchEvent(ui::TouchEvent* event) override {
    switch(event->type()) {
      case ui::ET_TOUCH_PRESSED:
        outstanding_touches_++;
        break;
      case ui::ET_TOUCH_RELEASED:
      case ui::ET_TOUCH_CANCELLED:
        outstanding_touches_--;
        break;
      default:
        break;
    }
  }

  int outstanding_touches() const { return outstanding_touches_; }

 private:
  int outstanding_touches_;
  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

// A wrapper around MenuMessageLoop that can be used to track whether a message
// loop is running or not.
class TestMenuMessageLoop : public MenuMessageLoop {
 public:
  explicit TestMenuMessageLoop(std::unique_ptr<MenuMessageLoop> original);
  ~TestMenuMessageLoop() override;

  bool is_running() const { return is_running_; }

  // MenuMessageLoop:
  void QuitNow() override;

 private:
  // MenuMessageLoop:
  void Run(MenuController* controller,
           Widget* owner,
           bool nested_menu) override;
  void ClearOwner() override;

  std::unique_ptr<MenuMessageLoop> original_;
  bool is_running_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuMessageLoop);
};

TestMenuMessageLoop::TestMenuMessageLoop(
    std::unique_ptr<MenuMessageLoop> original)
    : original_(std::move(original)) {
  DCHECK(original_);
}

TestMenuMessageLoop::~TestMenuMessageLoop() {}

void TestMenuMessageLoop::Run(MenuController* controller,
                              Widget* owner,
                              bool nested_menu) {
  is_running_ = true;
  original_->Run(controller, owner, nested_menu);
}

void TestMenuMessageLoop::QuitNow() {
  is_running_ = false;
  original_->QuitNow();
}

void TestMenuMessageLoop::ClearOwner() {
  original_->ClearOwner();
}

}  // namespace

class TestMenuItemViewShown : public MenuItemView {
 public:
  TestMenuItemViewShown(MenuDelegate* delegate) : MenuItemView(delegate) {
    submenu_ = new SubmenuViewShown(this);
  }
  ~TestMenuItemViewShown() override {}

  void SetController(MenuController* controller) {
    set_controller(controller);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuItemViewShown);
};

class MenuControllerTest : public ViewsTestBase {
 public:
  MenuControllerTest() : menu_controller_(nullptr) {
  }

  ~MenuControllerTest() override {}

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    Init();
  }

  void TearDown() override {
    owner_->CloseNow();
    DestroyMenuController();
    ViewsTestBase::TearDown();
  }

  void ReleaseTouchId(int id) {
    event_generator_->ReleaseTouchId(id);
  }

  void PressKey(ui::KeyboardCode key_code) {
    event_generator_->PressKey(key_code, 0);
  }

#if defined(OS_LINUX) && defined(USE_X11)
  void TestEventTargeter() {
    {
      // With the |ui::NullEventTargeter| instantiated and assigned we expect
      // the menu to not handle the key event.
      aura::ScopedWindowTargeter scoped_targeter(
          owner()->GetNativeWindow()->GetRootWindow(),
          std::unique_ptr<ui::EventTargeter>(new ui::NullEventTargeter));
      event_generator_->PressKey(ui::VKEY_ESCAPE, 0);
      EXPECT_EQ(MenuController::EXIT_NONE, menu_exit_type());
    }
    // Now that the targeter has been destroyed, expect to exit the menu
    // normally when hitting escape.
    event_generator_->PressKey(ui::VKEY_ESCAPE, 0);
    EXPECT_EQ(MenuController::EXIT_OUTERMOST, menu_exit_type());
  }
#endif  // defined(OS_LINUX) && defined(USE_X11)

  void TestAsynchronousNestedExitAll() {
    ASSERT_TRUE(test_message_loop_->is_running());

    std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
        new TestMenuControllerDelegate());

    menu_controller()->AddNestedDelegate(nested_delegate.get());
    menu_controller()->SetAsyncRun(true);

    int mouse_event_flags = 0;
    MenuItemView* run_result = menu_controller()->Run(
        owner(), nullptr, menu_item(), gfx::Rect(), MENU_ANCHOR_TOPLEFT, false,
        false, &mouse_event_flags);
    EXPECT_EQ(run_result, nullptr);

    // Exit all menus and check that the parent menu's message loop is
    // terminated.
    menu_controller()->Cancel(MenuController::EXIT_ALL);
    EXPECT_EQ(MenuController::EXIT_ALL, menu_controller()->exit_type());
    EXPECT_FALSE(test_message_loop_->is_running());
  }

  void TestAsynchronousNestedExitOutermost() {
    ASSERT_TRUE(test_message_loop_->is_running());

    std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
        new TestMenuControllerDelegate());

    menu_controller()->AddNestedDelegate(nested_delegate.get());
    menu_controller()->SetAsyncRun(true);

    int mouse_event_flags = 0;
    MenuItemView* run_result = menu_controller()->Run(
        owner(), nullptr, menu_item(), gfx::Rect(), MENU_ANCHOR_TOPLEFT, false,
        false, &mouse_event_flags);
    EXPECT_EQ(run_result, nullptr);

    // Exit the nested menu and check that the parent menu's message loop is
    // still running.
    menu_controller()->Cancel(MenuController::EXIT_OUTERMOST);
    EXPECT_EQ(MenuController::EXIT_NONE, menu_controller()->exit_type());
    EXPECT_TRUE(test_message_loop_->is_running());

    // Now, exit the parent menu and check that its message loop is terminated.
    menu_controller()->Cancel(MenuController::EXIT_OUTERMOST);
    EXPECT_EQ(MenuController::EXIT_OUTERMOST, menu_controller()->exit_type());
    EXPECT_FALSE(test_message_loop_->is_running());
  }

  // This nested an asynchronous delegate onto a menu with a nested message
  // loop, then kills the loop. Simulates the loop being killed not by
  // MenuController.
  void TestNestedMessageLoopKillsItself(
      TestMenuControllerDelegate* nested_delegate) {
    menu_controller_->AddNestedDelegate(nested_delegate);
    menu_controller_->SetAsyncRun(true);

    test_message_loop_->QuitNow();
  }

 protected:
  void SetPendingStateItem(MenuItemView* item) {
    menu_controller_->pending_state_.item = item;
    menu_controller_->pending_state_.submenu_open = true;
  }

  void ResetSelection() {
    menu_controller_->SetSelection(
        nullptr,
        MenuController::SELECTION_EXIT |
        MenuController::SELECTION_UPDATE_IMMEDIATELY);
  }

  void IncrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_DOWN);
  }

  void DecrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_UP);
  }

  void DestroyMenuControllerOnMenuClosed(TestMenuControllerDelegate* delegate) {
    // Unretained() is safe here as the test should outlive the delegate. If not
    // we want to know.
    delegate->set_on_menu_closed_callback(base::Bind(
        &MenuControllerTest::DestroyMenuController, base::Unretained(this)));
  }

  MenuItemView* FindInitialSelectableMenuItemDown(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindInitialSelectableMenuItemUp(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_UP);
  }

  MenuItemView* FindNextSelectableMenuItem(MenuItemView* parent,
                                           int index) {

    return menu_controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindPreviousSelectableMenuItem(MenuItemView* parent,
                                               int index) {
    return menu_controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_UP);
  }

  internal::MenuControllerDelegate* GetCurrentDelegate() {
    return menu_controller_->delegate_;
  }

  bool IsAsyncRun() { return menu_controller_->async_run_; }

  bool IsShowing() { return menu_controller_->showing_; }

  void SelectByChar(base::char16 character) {
    menu_controller_->SelectByChar(character);
  }

  void SetDropMenuItem(MenuItemView* target,
                       MenuDelegate::DropPosition position) {
    menu_controller_->SetDropMenuItem(target, position);
  }

  void SetIsCombobox(bool is_combobox) {
    menu_controller_->set_is_combobox(is_combobox);
  }

  void SetSelectionOnPointerDown(SubmenuView* source,
                                 const ui::LocatedEvent* event) {
    menu_controller_->SetSelectionOnPointerDown(source, event);
  }

  // Note that coordinates of events passed to MenuController must be in that of
  // the MenuScrollViewContainer.
  void ProcessMouseMoved(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMouseMoved(source, event);
  }

  void RunMenu() {
#if defined(USE_AURA)
    std::unique_ptr<MenuKeyEventHandler> key_event_handler(
        new MenuKeyEventHandler);
#endif

    menu_controller_->message_loop_depth_++;
    menu_controller_->RunMessageLoop(false);
    menu_controller_->message_loop_depth_--;
  }

  void Accept(MenuItemView* item, int event_flags) {
    menu_controller_->Accept(item, event_flags);
  }

  void InstallTestMenuMessageLoop() {
    test_message_loop_ =
        new TestMenuMessageLoop(std::move(menu_controller_->message_loop_));
    menu_controller_->message_loop_.reset(test_message_loop_);
  }

  Widget* owner() { return owner_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  TestMenuItemViewShown* menu_item() { return menu_item_.get(); }
  TestMenuControllerDelegate* menu_controller_delegate() {
    return menu_controller_delegate_.get();
  }
  MenuController* menu_controller() { return menu_controller_; }
  const MenuItemView* pending_state_item() const {
      return menu_controller_->pending_state_.item;
  }
  MenuController::ExitType menu_exit_type() const {
    return menu_controller_->exit_type_;
  }

  void AddButtonMenuItems() {
    menu_item()->SetBounds(0, 0, 200, 300);
    MenuItemView* item_view =
        menu_item()->AppendMenuItemWithLabel(5, base::ASCIIToUTF16("Five"));
    for (int i = 0; i < 3; ++i) {
      LabelButton* button =
          new LabelButton(nullptr, base::ASCIIToUTF16("Label"));
      // This is an in-menu button. Hence it must be always focusable.
      button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
      item_view->AddChildView(button);
    }
    menu_item()->GetSubmenu()->ShowAt(owner(), menu_item()->bounds(), false);
  }

  CustomButton* GetHotButton() {
    return menu_controller_->hot_button_;
  }

  void SetHotTrackedButton(CustomButton* hot_button) {
    menu_controller_->SetHotTrackedButton(hot_button);
  }

  void ExitMenuRun() {
    menu_controller_->SetExitType(MenuController::ExitType::EXIT_OUTERMOST);
    menu_controller_->ExitMenuRun();
  }

 private:
  void DestroyMenuController() {
    if (!menu_controller_)
      return;

    if (!owner_->IsClosed())
      owner_->RemoveObserver(menu_controller_);

    menu_controller_->showing_ = false;
    menu_controller_->owner_ = nullptr;
    delete menu_controller_;
    menu_controller_ = nullptr;
  }

  void Init() {
    owner_.reset(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    owner_->Init(params);
    event_generator_.reset(
        new ui::test::EventGenerator(owner_->GetNativeWindow()));
    owner_->Show();

    SetupMenuItem();
    SetupMenuController();
  }

  void SetupMenuItem() {
    menu_delegate_.reset(new TestMenuDelegate);
    menu_item_.reset(new TestMenuItemViewShown(menu_delegate_.get()));
    menu_item_->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
    menu_item_->AppendMenuItemWithLabel(2, base::ASCIIToUTF16("Two"));
    menu_item_->AppendMenuItemWithLabel(3, base::ASCIIToUTF16("Three"));
    menu_item_->AppendMenuItemWithLabel(4, base::ASCIIToUTF16("Four"));
  }

  void SetupMenuController() {
    menu_controller_delegate_.reset(new TestMenuControllerDelegate);
    menu_controller_ =
        new MenuController(true, menu_controller_delegate_.get());
    menu_controller_->owner_ = owner_.get();
    menu_controller_->showing_ = true;
    menu_controller_->SetSelection(
        menu_item_.get(), MenuController::SELECTION_UPDATE_IMMEDIATELY);
    menu_item_->SetController(menu_controller_);
  }

  std::unique_ptr<Widget> owner_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<TestMenuItemViewShown> menu_item_;
  std::unique_ptr<TestMenuControllerDelegate> menu_controller_delegate_;
  std::unique_ptr<MenuDelegate> menu_delegate_;
  MenuController* menu_controller_;
  TestMenuMessageLoop* test_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MenuControllerTest);
};

#if defined(OS_LINUX) && defined(USE_X11)
// Tests that an event targeter which blocks events will be honored by the menu
// event dispatcher.
TEST_F(MenuControllerTest, EventTargeter) {
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::TestEventTargeter,
                 base::Unretained(this)));
  RunMenu();
}
#endif  // defined(OS_LINUX) && defined(USE_X11)

#if defined(USE_X11)
// Tests that touch event ids are released correctly. See crbug.com/439051 for
// details. When the ids aren't managed correctly, we get stuck down touches.
TEST_F(MenuControllerTest, TouchIdsReleasedCorrectly) {
  TestEventHandler test_event_handler;
  owner()->GetNativeWindow()->GetRootWindow()->AddPreTargetHandler(
      &test_event_handler);

  std::vector<int> devices;
  devices.push_back(1);
  ui::SetUpTouchDevicesForTest(devices);

  event_generator()->PressTouchId(0);
  event_generator()->PressTouchId(1);
  event_generator()->ReleaseTouchId(0);

  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::ReleaseTouchId,
                 base::Unretained(this),
                 1));

  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::PressKey,
                 base::Unretained(this),
                 ui::VKEY_ESCAPE));

  RunMenu();

  EXPECT_EQ(MenuController::EXIT_OUTERMOST, menu_exit_type());
  EXPECT_EQ(0, test_event_handler.outstanding_touches());

  owner()->GetNativeWindow()->GetRootWindow()->RemovePreTargetHandler(
      &test_event_handler);
}
#endif  // defined(USE_X11)

// Tests that initial selected menu items are correct when items are enabled or
// disabled.
TEST_F(MenuControllerTest, InitialSelectedItem) {
  // Leave items "Two", "Three", and "Four" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  // The first selectable item should be item "Two".
  MenuItemView* first_selectable =
      FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  // The last selectable item should be item "Four".
  MenuItemView* last_selectable =
      FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(4, last_selectable->GetCommand());

  // Leave items "One" and "Two" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "Two".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // Leave only a single item "One" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "One".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(1, last_selectable->GetCommand());

  // Leave only a single item "Three" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "Three".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(3, first_selectable->GetCommand());
  // The last selectable item should be item "Three".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(3, last_selectable->GetCommand());

  // Leave only a single item ("Two") selected. It should be the first and the
  // last selectable item.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // There should be no next or previous selectable item since there is only a
  // single enabled item in the menu.
  EXPECT_EQ(nullptr, FindNextSelectableMenuItem(menu_item(), 1));
  EXPECT_EQ(nullptr, FindPreviousSelectableMenuItem(menu_item(), 1));

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and pressing 'Down' and 'Up' iterates over enabled
// items.
TEST_F(MenuControllerTest, NextSelectedItem) {
  // Disabling the item "Three" gets it skipped when using keyboard to navigate.
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);

  // Fake initial hot selection.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Move down in the menu.
  // Select next item.
  IncrementSelection();
  EXPECT_EQ(2, pending_state_item()->GetCommand());

  // Skip disabled item.
  IncrementSelection();
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Wrap around.
  IncrementSelection();
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Move up in the menu.
  // Wrap around.
  DecrementSelection();
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Skip disabled item.
  DecrementSelection();
  EXPECT_EQ(2, pending_state_item()->GetCommand());

  // Select previous item.
  DecrementSelection();
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and pressing 'Up' selects the last enabled menu item.
TEST_F(MenuControllerTest, PreviousSelectedItem) {
  // Disabling the item "Four" gets it skipped when using keyboard to navigate.
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);

  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Move up and select a previous (in our case the last enabled) item.
  DecrementSelection();
  EXPECT_EQ(3, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and calling SelectByChar works correctly.
TEST_F(MenuControllerTest, SelectByChar) {
  SetIsCombobox(true);

  // Handle null character should do nothing.
  SelectByChar(0);
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

TEST_F(MenuControllerTest, SelectChildButtonView) {
  AddButtonMenuItems();
  View* buttons_view = menu_item()->GetSubmenu()->child_at(4);
  ASSERT_NE(nullptr, buttons_view);
  CustomButton* button1 =
      CustomButton::AsCustomButton(buttons_view->child_at(0));
  ASSERT_NE(nullptr, button1);
  CustomButton* button2 =
      CustomButton::AsCustomButton(buttons_view->child_at(1));
  ASSERT_NE(nullptr, button2);
  CustomButton* button3 =
      CustomButton::AsCustomButton(buttons_view->child_at(2));
  ASSERT_NE(nullptr, button2);

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button1|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button2|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button3|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_TRUE(button3->IsHotTracked());

  // Move a mouse to hot track the |button1|.
  SubmenuView* sub_menu = menu_item()->GetSubmenu();
  gfx::Point location(button1->GetBoundsInScreen().CenterPoint());
  View::ConvertPointFromScreen(sub_menu->GetScrollViewContainer(), &location);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, event);

  // Incrementing selection should move hot tracking to the second button (next
  // after the first button).
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment selection twice to wrap around.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

TEST_F(MenuControllerTest, DeleteChildButtonView) {
  AddButtonMenuItems();

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  View* buttons_view = menu_item()->GetSubmenu()->child_at(4);
  ASSERT_NE(nullptr, buttons_view);
  CustomButton* button1 =
      CustomButton::AsCustomButton(buttons_view->child_at(0));
  ASSERT_NE(nullptr, button1);
  CustomButton* button2 =
      CustomButton::AsCustomButton(buttons_view->child_at(1));
  ASSERT_NE(nullptr, button2);
  CustomButton* button3 =
      CustomButton::AsCustomButton(buttons_view->child_at(2));
  ASSERT_NE(nullptr, button2);
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment twice to move selection to |button2|.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Delete |button2| while it is hot-tracked.
  // This should update MenuController via ViewHierarchyChanged and reset
  // |hot_button_|.
  delete button2;

  // Incrementing selection should now set hot-tracked item to |button1|.
  // It should not crash.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());
}

// Creates a menu with CustomButton child views, simulates running a nested
// menu and tests that existing the nested run restores hot-tracked child view.
TEST_F(MenuControllerTest, ChildButtonHotTrackedWhenNested) {
  AddButtonMenuItems();

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  View* buttons_view = menu_item()->GetSubmenu()->child_at(4);
  ASSERT_NE(nullptr, buttons_view);
  CustomButton* button1 =
      CustomButton::AsCustomButton(buttons_view->child_at(0));
  ASSERT_NE(nullptr, button1);
  CustomButton* button2 =
      CustomButton::AsCustomButton(buttons_view->child_at(1));
  ASSERT_NE(nullptr, button2);
  CustomButton* button3 =
      CustomButton::AsCustomButton(buttons_view->child_at(2));
  ASSERT_NE(nullptr, button2);
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment twice to move selection to |button2|.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());
  EXPECT_EQ(button2, GetHotButton());

  MenuController* controller = menu_controller();
  controller->SetAsyncRun(true);
  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);

  // |button2| should stay in hot-tracked state but menu controller should not
  // track it anymore (preventing resetting hot-tracked state when changing
  // selection while a nested run is active).
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(nullptr, GetHotButton());

  // Setting hot-tracked button while nested should get reverted when nested
  // menu run ends.
  SetHotTrackedButton(button1);
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_EQ(button1, GetHotButton());

  ExitMenuRun();
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(button2, GetHotButton());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when Accept is called.
TEST_F(MenuControllerTest, AsynchronousAccept) {
  MenuController* controller = menu_controller();
  controller->SetAsyncRun(true);

  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  const int kEventFlags = 42;
  Accept(accepted, kEventFlags);

  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(accepted, delegate->on_menu_closed_menu());
  EXPECT_EQ(kEventFlags, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when CancelAll is called.
TEST_F(MenuControllerTest, AsynchronousCancelAll) {
  MenuController* controller = menu_controller();
  controller->SetAsyncRun(true);

  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  controller->CancelAll();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(0, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::EXIT_ALL, controller->exit_type());
}

// Tests that an asynchrnous menu nested within a synchronous menu restores the
// previous MenuControllerDelegate and synchronous settings.
TEST_F(MenuControllerTest, AsynchronousNestedDelegate) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  ASSERT_FALSE(IsAsyncRun());
  controller->AddNestedDelegate(nested_delegate.get());
  controller->SetAsyncRun(true);

  EXPECT_TRUE(IsAsyncRun());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);

  controller->CancelAll();
  EXPECT_FALSE(IsAsyncRun());
  EXPECT_EQ(delegate, GetCurrentDelegate());
  EXPECT_EQ(0, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::EXIT_ALL, controller->exit_type());
}

// Tests that dropping within an asynchronous menu stops the menu from showing
// and does not notify the controller.
TEST_F(MenuControllerTest, AsynchronousPerformDrop) {
  MenuController* controller = menu_controller();
  controller->SetAsyncRun(true);
  SubmenuView* source = menu_item()->GetSubmenu();
  MenuItemView* target = source->GetMenuItemAt(0);

  SetDropMenuItem(target, MenuDelegate::DropPosition::DROP_AFTER);

  ui::OSExchangeData drop_data;
  gfx::Rect bounds(target->bounds());
  gfx::Point location(bounds.x(), bounds.y());
  ui::DropTargetEvent target_event(drop_data, location, location,
                                   ui::DragDropTypes::DRAG_MOVE);
  controller->OnPerformDrop(source, target_event);

  TestMenuDelegate* menu_delegate =
      static_cast<TestMenuDelegate*>(target->GetDelegate());
  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_TRUE(menu_delegate->on_perform_drop_called());
  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(0, controller_delegate->on_menu_closed_called());
}

// Tests that dragging within an asynchronous menu notifies the
// MenuControllerDelegate for shutdown.
TEST_F(MenuControllerTest, AsynchronousDragComplete) {
  MenuController* controller = menu_controller();
  controller->SetAsyncRun(true);

  controller->OnDragWillStart();
  controller->OnDragComplete(true);

  EXPECT_FALSE(controller->drag_in_progress());
  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_EQ(1, controller_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, controller_delegate->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            controller_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::EXIT_ALL, controller->exit_type());
}

// Tets that an asynchronous menu nested within an asynchronous menu closes both
// menus, and notifies both delegates.
TEST_F(MenuControllerTest, DoubleAsynchronousNested) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  ASSERT_FALSE(IsAsyncRun());
  // Sets the run created in SetUp
  controller->SetAsyncRun(true);

  // Nested run
  controller->AddNestedDelegate(nested_delegate.get());
  controller->SetAsyncRun(true);
  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);

  controller->CancelAll();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that an asynchronous menu nested within a synchronous menu does not
// crash when trying to repost events that occur outside of the bounds of the
// menu. Instead a proper shutdown should occur.
TEST_F(MenuControllerTest, AsynchronousRepostEvent) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  ASSERT_FALSE(IsAsyncRun());

  controller->AddNestedDelegate(nested_delegate.get());
  controller->SetAsyncRun(true);

  EXPECT_TRUE(IsAsyncRun());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, item, gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                      false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);

  // Show a sub menu to target with a pointer selection. However have the event
  // occur outside of the bounds of the entire menu.
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), item->bounds(), false);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(sub_menu, &event);

  EXPECT_FALSE(IsAsyncRun());
  EXPECT_EQ(delegate, GetCurrentDelegate());
  EXPECT_EQ(0, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::EXIT_ALL, controller->exit_type());
}

// Tests that an asynchronous menu reposts touch events that occur outside of
// the bounds of the menu, and that the menu closes.
TEST_F(MenuControllerTest, AsynchronousTouchEventRepostEvent) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  controller->SetAsyncRun(true);

  // Show a sub menu to target with a touch event. However have the event occur
  // outside of the bounds of the entire menu.
  MenuItemView* item = menu_item();
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), item->bounds(), false);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::TouchEvent event(ui::ET_TOUCH_PRESSED, location, 0, 0,
                       ui::EventTimeForNow(), 0, 0, 0, 0);
  controller->OnTouchEvent(sub_menu, &event);

  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(0, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::EXIT_ALL, controller->exit_type());
}

// Tests that if you exit all menus when an asynchrnous menu is nested within a
// synchronous menu, the message loop for the parent menu finishes running.
TEST_F(MenuControllerTest, AsynchronousNestedExitAll) {
  InstallTestMenuMessageLoop();

  base::MessageLoopForUI::current()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MenuControllerTest::TestAsynchronousNestedExitAll,
                            base::Unretained(this)));

  RunMenu();
}

// Tests that if you exit the nested menu when an asynchrnous menu is nested
// within a synchronous menu, the message loop for the parent menu remains
// running.
TEST_F(MenuControllerTest, AsynchronousNestedExitOutermost) {
  InstallTestMenuMessageLoop();

  base::MessageLoopForUI::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::TestAsynchronousNestedExitOutermost,
                 base::Unretained(this)));

  RunMenu();
}

// Tests that having the MenuController deleted during RepostEvent does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, AsynchronousRepostEventDeletesController) {
  MenuController* controller = menu_controller();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  ASSERT_FALSE(IsAsyncRun());

  controller->AddNestedDelegate(nested_delegate.get());
  controller->SetAsyncRun(true);

  EXPECT_TRUE(IsAsyncRun());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, item, gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                      false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);

  // Show a sub menu to target with a pointer selection. However have the event
  // occur outside of the bounds of the entire menu.
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), item->bounds(), true);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // This will lead to MenuController being deleted during the event repost.
  // The remainder of this test, and TearDown should not crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());
  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(sub_menu, &event);

  // Close to remove observers before test TearDown
  sub_menu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that having the MenuController deleted during OnGestureEvent does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, AsynchronousGestureDeletesController) {
  MenuController* controller = menu_controller();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());
  ASSERT_FALSE(IsAsyncRun());

  controller->AddNestedDelegate(nested_delegate.get());
  controller->SetAsyncRun(true);

  EXPECT_TRUE(IsAsyncRun());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, item, gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                      false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);

  // Show a sub menu to target with a tap event.
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), gfx::Rect(0, 0, 100, 100), true);

  gfx::Point location(sub_menu->bounds().CenterPoint());
  ui::GestureEvent event(location.x(), location.y(), 0, ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  // This will lead to MenuController being deleted during the processing of the
  // gesture event. The remainder of this test, and TearDown should not crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());
  controller->OnGestureEvent(sub_menu, &event);

  // Close to remove observers before test TearDown
  sub_menu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that when an asynchronous menu is nested, and the nested message loop
// is kill not by the MenuController, that the nested menu is notified of
// destruction.
TEST_F(MenuControllerTest, NestedMessageLoopDiesWithNestedMenu) {
  menu_controller()->CancelAll();
  InstallTestMenuMessageLoop();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());
  // This will nest an asynchronous menu, and then kill the nested message loop.
  base::MessageLoopForUI::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::TestNestedMessageLoopKillsItself,
                 base::Unretained(this), nested_delegate.get()));

  int result_event_flags = 0;
  // This creates a nested message loop.
  EXPECT_EQ(nullptr, menu_controller()->Run(owner(), nullptr, menu_item(),
                                            gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                                            false, false, &result_event_flags));
  EXPECT_FALSE(menu_controller_delegate()->on_menu_closed_called());
  EXPECT_TRUE(nested_delegate->on_menu_closed_called());
}

}  // namespace test
}  // namespace views
