// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/env_input_state_controller.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"

namespace aura {

void EnvInputStateController::UpdateStateForMouseEvent(
    const Window* window,
    const ui::MouseEvent& event) {
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      Env::GetInstance()->set_mouse_button_flags(event.button_flags());
      break;
    case ui::ET_MOUSE_RELEASED:
      Env::GetInstance()->set_mouse_button_flags(
          event.button_flags() & ~event.changed_button_flags());
      break;
    default:
      break;
  }

  if (event.type() != ui::ET_MOUSE_CAPTURE_CHANGED &&
      !(event.flags() & ui::EF_IS_SYNTHESIZED)) {
    SetLastMouseLocation(window, event.root_location());
  }
}

void EnvInputStateController::UpdateStateForTouchEvent(
    const ui::TouchEvent& event) {
  switch (event.type()) {
    case ui::ET_TOUCH_PRESSED:
      touch_ids_down_ |= (1 << event.touch_id());
      Env::GetInstance()->set_touch_down(touch_ids_down_ != 0);
      break;

    // Handle ET_TOUCH_CANCELLED only if it has a native event.
    case ui::ET_TOUCH_CANCELLED:
      if (!event.HasNativeEvent())
        break;
    // fallthrough
    case ui::ET_TOUCH_RELEASED:
      touch_ids_down_ =
          (touch_ids_down_ | (1 << event.touch_id())) ^ (1 << event.touch_id());
      Env::GetInstance()->set_touch_down(touch_ids_down_ != 0);
      break;

    case ui::ET_TOUCH_MOVED:
      break;

    default:
      NOTREACHED();
      break;
  }
}

void EnvInputStateController::SetLastMouseLocation(
    const Window* root_window,
    const gfx::Point& location_in_root) const {
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(root_window);
  if (client) {
    gfx::Point location_in_screen = location_in_root;
    client->ConvertPointToScreen(root_window, &location_in_screen);
    Env::GetInstance()->set_last_mouse_location(location_in_screen);
  } else {
    Env::GetInstance()->set_last_mouse_location(location_in_root);
  }
}

}  // namespace aura
