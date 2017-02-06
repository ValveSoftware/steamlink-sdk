// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"

namespace aura {
namespace test {
namespace {

class UIControlsOzone : public ui_controls::UIControlsAura {
 public:
  UIControlsOzone(WindowTreeHost* host) : host_(host) {}

  bool SendKeyPress(gfx::NativeWindow window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override {
    return SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, base::Closure());
  }
  bool SendKeyPressNotifyWhenDone(
      gfx::NativeWindow window,
      ui::KeyboardCode key,
      bool control,
      bool shift,
      bool alt,
      bool command,
      const base::Closure& closure) override {
    int flags = button_down_mask_;

    if (control) {
      flags |= ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, flags);
    }

    if (shift) {
      flags |= ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, flags);
    }

    if (alt) {
      flags |= ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_MENU, flags);
    }

    if (command) {
      flags |= ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LWIN, flags);
    }

    PostKeyEvent(ui::ET_KEY_PRESSED, key, flags);
    PostKeyEvent(ui::ET_KEY_RELEASED, key, flags);

    if (alt) {
      flags &= ~ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_MENU, flags);
    }

    if (shift) {
      flags &= ~ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, flags);
    }

    if (control) {
      flags &= ~ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, flags);
    }

    if (command) {
      flags &= ~ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_LWIN, flags);
    }

    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }

  bool SendMouseMove(long screen_x, long screen_y) override {
    return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::Closure());
  }
  bool SendMouseMoveNotifyWhenDone(
      long screen_x,
      long screen_y,
      const base::Closure& closure) override {
    gfx::Point root_location(screen_x, screen_y);
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(host_->window());
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(host_->window(),
                                                     &root_location);
    }

    gfx::Point host_location = root_location;
    host_->ConvertPointToHost(&host_location);

    ui::EventType event_type;

    if (button_down_mask_)
      event_type = ui::ET_MOUSE_DRAGGED;
    else
      event_type = ui::ET_MOUSE_MOVED;

    PostMouseEvent(event_type, host_location, 0, 0);

    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }
  bool SendMouseEvents(ui_controls::MouseButton type, int state) override {
    return SendMouseEventsNotifyWhenDone(type, state, base::Closure());
  }
  bool SendMouseEventsNotifyWhenDone(
      ui_controls::MouseButton type,
      int state,
      const base::Closure& closure) override {
    gfx::Point root_location = aura::Env::GetInstance()->last_mouse_location();
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(host_->window());
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(host_->window(),
                                                     &root_location);
    }

    gfx::Point host_location = root_location;
    host_->ConvertPointToHost(&host_location);

    int flag = 0;

    switch (type) {
      case ui_controls::LEFT:
        flag = ui::EF_LEFT_MOUSE_BUTTON;
        break;
      case ui_controls::MIDDLE:
        flag = ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      case ui_controls::RIGHT:
        flag = ui::EF_RIGHT_MOUSE_BUTTON;
        break;
      default:
        NOTREACHED();
        break;
    }

    if (state & ui_controls::DOWN) {
      button_down_mask_ |= flag;
      PostMouseEvent(ui::ET_MOUSE_PRESSED, host_location,
                     button_down_mask_ | flag, flag);
    }
    if (state & ui_controls::UP) {
      button_down_mask_ &= ~flag;
      PostMouseEvent(ui::ET_MOUSE_RELEASED, host_location,
                     button_down_mask_ | flag, flag);
    }

    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }
  bool SendMouseClick(ui_controls::MouseButton type) override {
    return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN);
  }
  void RunClosureAfterAllPendingUIEvents(
      const base::Closure& closure) override {
    if (!closure.is_null())
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, closure);
  }

 private:
  void SendEventToProcessor(ui::Event* event) {
    ui::EventSourceTestApi event_source_test(host_->GetEventSource());
    ui::EventDispatchDetails details =
        event_source_test.SendEventToProcessor(event);
    if (details.dispatcher_destroyed)
      return;
  }

  void PostKeyEvent(ui::EventType type, ui::KeyboardCode key_code, int flags) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&UIControlsOzone::PostKeyEventTask,
                              base::Unretained(this), type, key_code, flags));
  }

  void PostKeyEventTask(ui::EventType type,
                        ui::KeyboardCode key_code,
                        int flags) {
    // Do not rewrite injected events. See crbug.com/136465.
    flags |= ui::EF_FINAL;

    ui::KeyEvent key_event(type, key_code, flags);
    SendEventToProcessor(&key_event);
  }

  void PostMouseEvent(ui::EventType type,
                      const gfx::Point& host_location,
                      int flags,
                      int changed_button_flags) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&UIControlsOzone::PostMouseEventTask, base::Unretained(this),
                   type, host_location, flags, changed_button_flags));
  }

  void PostMouseEventTask(ui::EventType type,
                          const gfx::Point& host_location,
                          int flags,
                          int changed_button_flags) {
    ui::MouseEvent mouse_event(type, host_location, host_location,
                               ui::EventTimeForNow(), flags,
                               changed_button_flags);

    // This hack is necessary to set the repeat count for clicks.
    ui::MouseEvent mouse_event2(&mouse_event);

    SendEventToProcessor(&mouse_event2);
  }

  WindowTreeHost* host_;

  // Mask of the mouse buttons currently down.
  unsigned button_down_mask_ = 0;

  DISALLOW_COPY_AND_ASSIGN(UIControlsOzone);
};

}  // namespace

ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsOzone(host);
}

}  // namespace test
}  // namespace aura
