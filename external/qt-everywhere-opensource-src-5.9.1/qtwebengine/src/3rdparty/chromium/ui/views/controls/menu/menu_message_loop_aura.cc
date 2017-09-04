// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_loop_aura.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
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


using aura::client::ScreenPositionClient;

namespace views {

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

MenuMessageLoopAura::MenuMessageLoopAura() {}

MenuMessageLoopAura::~MenuMessageLoopAura() {}

void MenuMessageLoopAura::Run() {
  // It is possible for the same MenuMessageLoopAura to start a nested
  // message-loop while it is already running a nested loop. So make sure the
  // quit-closure gets reset to the outer loop's quit-closure once the innermost
  // loop terminates.
  base::AutoReset<base::Closure> reset_quit_closure(&message_loop_quit_,
                                                    base::Closure());

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

}  // namespace views
