// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gamepad_user_gesture.h"

#include <algorithm>

#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

bool GamepadsHaveUserGesture(const blink::WebGamepads& gamepads) {
  for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; i++) {
    const blink::WebGamepad& pad = gamepads.items[i];

    // If the device is physically connected, then check the primary 4 buttons
    // to see if there is currently an intentional user action.
    if (pad.connected) {
      const unsigned kPrimaryInteractionButtons = 4;
      unsigned buttons_to_check = std::min(pad.buttonsLength,
                                           kPrimaryInteractionButtons);
      for (unsigned button_index = 0; button_index < buttons_to_check;
           button_index++) {
        if (pad.buttons[button_index].pressed)
          return true;
      }
    }
  }
  return false;
}

}  // namespace content
