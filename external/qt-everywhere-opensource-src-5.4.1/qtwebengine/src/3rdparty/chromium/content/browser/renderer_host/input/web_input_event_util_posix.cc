// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_util_posix.h"

namespace content {

ui::KeyboardCode GetWindowsKeyCodeWithoutLocation(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
      return ui::VKEY_CONTROL;
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
    return ui::VKEY_SHIFT;
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
      return ui::VKEY_MENU;
    default:
      return key_code;
  }
}

blink::WebInputEvent::Modifiers GetLocationModifiersFromWindowsKeyCode(
    ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_LCONTROL:
    case ui::VKEY_LSHIFT:
    case ui::VKEY_LMENU:
    case ui::VKEY_LWIN:
      return blink::WebKeyboardEvent::IsLeft;
    case ui::VKEY_RCONTROL:
    case ui::VKEY_RSHIFT:
    case ui::VKEY_RMENU:
    case ui::VKEY_RWIN:
      return blink::WebKeyboardEvent::IsRight;
    default:
      return static_cast<blink::WebInputEvent::Modifiers>(0);
  }
}

}  // namespace content
