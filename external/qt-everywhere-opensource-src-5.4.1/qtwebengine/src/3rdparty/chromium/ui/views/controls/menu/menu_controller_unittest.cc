// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/run_loop.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/events/event_targeter.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/wm/public/dispatcher_client.h"

#if defined(OS_WIN)
#include "base/message_loop/message_pump_dispatcher.h"
#elif defined(USE_X11)
#include <X11/Xlib.h>
#undef Bool
#undef None
#include "ui/events/test/events_test_utils_x11.h"
#elif defined(USE_OZONE)
#include "ui/events/event.h"
#endif

namespace views {

namespace {

class TestMenuItemView : public MenuItemView {
 public:
  TestMenuItemView() : MenuItemView(NULL) {}
  virtual ~TestMenuItemView() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuItemView);
};

class TestPlatformEventSource : public ui::PlatformEventSource {
 public:
  TestPlatformEventSource() {}
  virtual ~TestPlatformEventSource() {}

  uint32_t Dispatch(const ui::PlatformEvent& event) {
    return DispatchEvent(event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPlatformEventSource);
};

class TestNullTargeter : public ui::EventTargeter {
 public:
  TestNullTargeter() {}
  virtual ~TestNullTargeter() {}

  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNullTargeter);
};

class TestDispatcherClient : public aura::client::DispatcherClient {
 public:
  TestDispatcherClient() : dispatcher_(NULL) {}
  virtual ~TestDispatcherClient() {}

  base::MessagePumpDispatcher* dispatcher() {
    return dispatcher_;
  }

  // aura::client::DispatcherClient:
  virtual void PrepareNestedLoopClosures(
      base::MessagePumpDispatcher* dispatcher,
      base::Closure* run_closure,
      base::Closure* quit_closure) OVERRIDE {
    scoped_ptr<base::RunLoop> run_loop(new base::RunLoop());
    *quit_closure = run_loop->QuitClosure();
    *run_closure = base::Bind(&TestDispatcherClient::RunNestedDispatcher,
                              base::Unretained(this),
                              base::Passed(&run_loop),
                              dispatcher);
  }

 private:
  void RunNestedDispatcher(scoped_ptr<base::RunLoop> run_loop,
                           base::MessagePumpDispatcher* dispatcher) {
    base::AutoReset<base::MessagePumpDispatcher*> reset_dispatcher(&dispatcher_,
                                                                   dispatcher);
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    run_loop->Run();
  }

  base::MessagePumpDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(TestDispatcherClient);
};

}  // namespace

class MenuControllerTest : public ViewsTestBase {
 public:
  MenuControllerTest() : controller_(NULL) {}
  virtual ~MenuControllerTest() {
    ResetMenuController();
  }

  // Dispatches |count| number of items, each in a separate iteration of the
  // message-loop, by posting a task.
  void Step3_DispatchEvents(int count) {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    controller_->exit_type_ = MenuController::EXIT_ALL;

    DispatchEvent();
    if (count) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&MenuControllerTest::Step3_DispatchEvents,
                     base::Unretained(this),
                     count - 1));
    } else {
      EXPECT_TRUE(run_loop_->running());
      run_loop_->Quit();
    }
  }

  // Runs a nested message-loop that does not involve the menu itself.
  void Step2_RunNestedLoop() {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::Step3_DispatchEvents,
                   base::Unretained(this),
                   3));
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void Step1_RunMenu() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::Step2_RunNestedLoop,
                   base::Unretained(this)));
    scoped_ptr<Widget> owner(CreateOwnerWidget());
    RunMenu(owner.get());
  }

  scoped_ptr<Widget> CreateOwnerWidget() {
    scoped_ptr<Widget> widget(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget->Init(params);
    widget->Show();

    aura::client::SetDispatcherClient(
        widget->GetNativeWindow()->GetRootWindow(), &dispatcher_client_);
    return widget.Pass();
  }

  void RunMenu(views::Widget* owner) {
    scoped_ptr<TestMenuItemView> menu_item(new TestMenuItemView);
    ResetMenuController();
    controller_ = new MenuController(NULL, true, NULL);
    controller_->owner_ = owner;
    controller_->showing_ = true;
    controller_->SetSelection(menu_item.get(),
                              MenuController::SELECTION_UPDATE_IMMEDIATELY);
    controller_->RunMessageLoop(false);
  }

#if defined(USE_X11)
  void DispatchEscapeAndExpect(MenuController::ExitType exit_type) {
    ui::ScopedXI2Event key_event;
    key_event.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0);
    event_source_.Dispatch(key_event);
    EXPECT_EQ(exit_type, controller_->exit_type());
    controller_->exit_type_ = MenuController::EXIT_ALL;
    DispatchEvent();
  }
#endif

  void DispatchEvent() {
#if defined(USE_X11)
    XEvent xevent;
    memset(&xevent, 0, sizeof(xevent));
    event_source_.Dispatch(&xevent);
#elif defined(OS_WIN)
    MSG msg;
    memset(&msg, 0, sizeof(MSG));
    dispatcher_client_.dispatcher()->Dispatch(msg);
#elif defined(USE_OZONE)
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0, true);
    dispatcher_client_.dispatcher()->Dispatch(&event);
#else
#error Unsupported platform
#endif
  }

 private:
  void ResetMenuController() {
    if (controller_) {
      // These properties are faked by RunMenu for the purposes of testing and
      // need to be undone before we call the destructor.
      controller_->owner_ = NULL;
      controller_->showing_ = false;
      delete controller_;
      controller_ = NULL;
    }
  }

  // A weak pointer to the MenuController owned by this class.
  MenuController* controller_;
  scoped_ptr<base::RunLoop> run_loop_;
  TestPlatformEventSource event_source_;
  TestDispatcherClient dispatcher_client_;

  DISALLOW_COPY_AND_ASSIGN(MenuControllerTest);
};

TEST_F(MenuControllerTest, Basic) {
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(
      base::MessageLoop::current());
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::Step1_RunMenu, base::Unretained(this)));
}

#if defined(OS_LINUX) && defined(USE_X11)
// Tests that an event targeter which blocks events will be honored by the menu
// event dispatcher.
TEST_F(MenuControllerTest, EventTargeter) {
  {
    // Verify that the menu handles the escape key under normal circumstances.
    scoped_ptr<Widget> owner(CreateOwnerWidget());
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::DispatchEscapeAndExpect,
                   base::Unretained(this),
                   MenuController::EXIT_OUTERMOST));
    RunMenu(owner.get());
  }

  {
    // With the NULL targeter instantiated and assigned we expect the menu to
    // not handle the key event.
    scoped_ptr<Widget> owner(CreateOwnerWidget());
    aura::ScopedWindowTargeter scoped_targeter(
        owner->GetNativeWindow()->GetRootWindow(),
        scoped_ptr<ui::EventTargeter>(new TestNullTargeter));
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::DispatchEscapeAndExpect,
                   base::Unretained(this),
                   MenuController::EXIT_NONE));
    RunMenu(owner.get());
  }
}
#endif

}  // namespace views
