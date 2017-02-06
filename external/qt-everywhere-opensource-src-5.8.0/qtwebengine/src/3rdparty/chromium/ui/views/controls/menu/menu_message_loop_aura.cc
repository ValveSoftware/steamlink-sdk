// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_loop_aura.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_key_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/drag_drop_client.h"


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

  ~ActivationChangeObserverImpl() override { Cleanup(); }

  // aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override {
    if (!controller_->drag_in_progress())
      controller_->CancelAll();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override { Cleanup(); }

  // ui::EventHandler:
  void OnCancelMode(ui::CancelModeEvent* event) override {
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

// static
void MenuMessageLoop::RepostEventToWindow(const ui::LocatedEvent* event,
                                          gfx::NativeWindow window,
                                          const gfx::Point& screen_loc) {
  aura::Window* root = window->GetRootWindow();
  aura::client::ScreenPositionClient* spc =
      aura::client::GetScreenPositionClient(root);
  if (!spc)
    return;

  gfx::Point root_loc(screen_loc);
  spc->ConvertPointFromScreen(root, &root_loc);

  std::unique_ptr<ui::Event> clone = ui::Event::Clone(*event);
  std::unique_ptr<ui::LocatedEvent> located_event(
      static_cast<ui::LocatedEvent*>(clone.release()));
  located_event->set_location(root_loc);
  located_event->set_root_location(root_loc);

  root->GetHost()->dispatcher()->RepostEvent(located_event.get());
}

MenuMessageLoopAura::MenuMessageLoopAura() : owner_(nullptr) {}

MenuMessageLoopAura::~MenuMessageLoopAura() {}

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

  std::unique_ptr<ActivationChangeObserverImpl> observer;
  if (root) {
    if (!nested_menu)
      observer.reset(new ActivationChangeObserverImpl(controller, root));
  }

  base::MessageLoop* loop = base::MessageLoop::current();
  base::MessageLoop::ScopedNestableTaskAllower allow(loop);
  base::RunLoop run_loop;
  message_loop_quit_ = run_loop.QuitClosure();

  run_loop.Run();
}

void MenuMessageLoopAura::QuitNow() {
  CHECK(!message_loop_quit_.is_null());
  message_loop_quit_.Run();

#if !defined(OS_WIN)
  // Ask PlatformEventSource to stop dispatching events in this message loop
  // iteration. We want our menu's loop to return before the next event.
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->StopCurrentEventStream();
#endif
}

void MenuMessageLoopAura::ClearOwner() {
  owner_ = NULL;
}

}  // namespace views
