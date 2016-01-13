// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_loop_aura.h"

#if defined(OS_WIN)
#include <windowsx.h>
#endif

#include "base/run_loop.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/dispatcher_client.h"
#include "ui/wm/public/drag_drop_client.h"

#if defined(OS_WIN)
#include "ui/base/win/internal_constants.h"
#include "ui/views/controls/menu/menu_message_pump_dispatcher_win.h"
#include "ui/views/win/hwnd_util.h"
#else
#include "ui/views/controls/menu/menu_event_dispatcher_linux.h"
#endif

using aura::client::ScreenPositionClient;

namespace views {

namespace {

aura::Window* GetOwnerRootWindow(views::Widget* owner) {
  return owner ? owner->GetNativeWindow()->GetRootWindow() : NULL;
}

// ActivationChangeObserverImpl is used to observe activation changes and close
// the menu. Additionally it listens for the root window to be destroyed and
// cancel the menu as well.
class ActivationChangeObserverImpl
    : public aura::client::ActivationChangeObserver,
      public aura::WindowObserver,
      public ui::EventHandler {
 public:
  ActivationChangeObserverImpl(MenuController* controller, aura::Window* root)
      : controller_(controller), root_(root) {
    aura::client::GetActivationClient(root_)->AddObserver(this);
    root_->AddObserver(this);
    root_->AddPreTargetHandler(this);
  }

  virtual ~ActivationChangeObserverImpl() { Cleanup(); }

  // aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE {
    if (!controller_->drag_in_progress())
      controller_->CancelAll();
  }

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE { Cleanup(); }

  // ui::EventHandler:
  virtual void OnCancelMode(ui::CancelModeEvent* event) OVERRIDE {
    controller_->CancelAll();
  }

 private:
  void Cleanup() {
    if (!root_)
      return;
    // The ActivationClient may have been destroyed by the time we get here.
    aura::client::ActivationClient* client =
        aura::client::GetActivationClient(root_);
    if (client)
      client->RemoveObserver(this);
    root_->RemovePreTargetHandler(this);
    root_->RemoveObserver(this);
    root_ = NULL;
  }

  MenuController* controller_;
  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(ActivationChangeObserverImpl);
};

}  // namespace

// static
MenuMessageLoop* MenuMessageLoop::Create() {
  return new MenuMessageLoopAura;
}

MenuMessageLoopAura::MenuMessageLoopAura() : owner_(NULL) {
}

MenuMessageLoopAura::~MenuMessageLoopAura() {
}

void MenuMessageLoopAura::RepostEventToWindow(const ui::LocatedEvent& event,
                                              gfx::NativeWindow window,
                                              const gfx::Point& screen_loc) {
  aura::Window* root = window->GetRootWindow();
  ScreenPositionClient* spc = aura::client::GetScreenPositionClient(root);
  if (!spc)
    return;

  gfx::Point root_loc(screen_loc);
  spc->ConvertPointFromScreen(root, &root_loc);

  ui::MouseEvent clone(static_cast<const ui::MouseEvent&>(event));
  clone.set_location(root_loc);
  clone.set_root_location(root_loc);
  root->GetHost()->dispatcher()->RepostEvent(clone);
}

void MenuMessageLoopAura::Run(MenuController* controller,
                              Widget* owner,
                              bool nested_menu) {
  // |owner_| may be NULL.
  owner_ = owner;
  aura::Window* root = GetOwnerRootWindow(owner_);
  // It is possible for the same MenuMessageLoopAura to start a nested
  // message-loop while it is already running a nested loop. So make sure the
  // quit-closure gets reset to the outer loop's quit-closure once the innermost
  // loop terminates.
  base::AutoReset<base::Closure> reset_quit_closure(&message_loop_quit_,
                                                    base::Closure());

#if defined(OS_WIN)
  internal::MenuMessagePumpDispatcher nested_dispatcher(controller);
  if (root) {
    scoped_ptr<ActivationChangeObserverImpl> observer;
    if (!nested_menu)
      observer.reset(new ActivationChangeObserverImpl(controller, root));
    aura::client::DispatcherRunLoop run_loop(
        aura::client::GetDispatcherClient(root), &nested_dispatcher);
    message_loop_quit_ = run_loop.QuitClosure();
    run_loop.Run();
  } else {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    base::RunLoop run_loop(&nested_dispatcher);
    message_loop_quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }
#else
  internal::MenuEventDispatcher event_dispatcher(controller);
  scoped_ptr<ui::ScopedEventDispatcher> old_dispatcher =
      nested_dispatcher_.Pass();
  if (ui::PlatformEventSource::GetInstance()) {
    nested_dispatcher_ =
        ui::PlatformEventSource::GetInstance()->OverrideDispatcher(
            &event_dispatcher);
  }
  if (root) {
    scoped_ptr<ActivationChangeObserverImpl> observer;
    if (!nested_menu)
      observer.reset(new ActivationChangeObserverImpl(controller, root));
    aura::client::DispatcherRunLoop run_loop(
        aura::client::GetDispatcherClient(root), NULL);
    message_loop_quit_ = run_loop.QuitClosure();
    run_loop.Run();
  } else {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    base::RunLoop run_loop;
    message_loop_quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  nested_dispatcher_ = old_dispatcher.Pass();
#endif
}

bool MenuMessageLoopAura::ShouldQuitNow() const {
  aura::Window* root = GetOwnerRootWindow(owner_);
  return !aura::client::GetDragDropClient(root) ||
         !aura::client::GetDragDropClient(root)->IsDragDropInProgress();
}

void MenuMessageLoopAura::QuitNow() {
  CHECK(!message_loop_quit_.is_null());
  message_loop_quit_.Run();
  // Restore the previous dispatcher.
  nested_dispatcher_.reset();
}

void MenuMessageLoopAura::ClearOwner() {
  owner_ = NULL;
}

}  // namespace views
