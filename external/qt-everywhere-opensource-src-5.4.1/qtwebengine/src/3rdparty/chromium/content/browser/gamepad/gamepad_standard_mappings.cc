// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_standard_mappings.h"

namespace content {

blink::WebGamepadButton AxisToButton(float input) {
  float value = (input + 1.f) / 2.f;
  return blink::WebGamepadButton(
    value > kDefaultButtonPressedThreshold, value);
}

blink::WebGamepadButton AxisNegativeAsButton(float input) {
  float value = (input < -0.5f) ? 1.f : 0.f;
  return blink::WebGamepadButton(
    value > kDefaultButtonPressedThreshold, value);
}

blink::WebGamepadButton AxisPositiveAsButton(float input) {
  float value = (input > 0.5f) ? 1.f : 0.f;
  return blink::WebGamepadButton(
    value > kDefaultButtonPressedThreshold, value);
}

blink::WebGamepadButton ButtonFromButtonAndAxis(
    blink::WebGamepadButton button, float axis) {
  float value = (axis + 1.f) / 2.f;
  return blink::WebGamepadButton(button.pressed, value);
}

void DpadFromAxis(blink::WebGamepad* mapped, float dir) {
  bool up = false;
  bool right = false;
  bool down = false;
  bool left = false;

  // Dpad is mapped as a direction on one axis, where -1 is up and it
  // increases clockwise to 1, which is up + left. It's set to a large (> 1.f)
  // number when nothing is depressed, except on start up, sometimes it's 0.0
  // for no data, rather than the large number.
  if (dir != 0.0f) {
    up = (dir >= -1.f && dir < -0.7f) || (dir >= .95f && dir <= 1.f);
    right = dir >= -.75f && dir < -.1f;
    down = dir >= -.2f && dir < .45f;
    left = dir >= .4f && dir <= 1.f;
  }

  mapped->buttons[kButtonDpadUp].pressed = up;
  mapped->buttons[kButtonDpadUp].value = up ? 1.f : 0.f;
  mapped->buttons[kButtonDpadRight].pressed = right;
  mapped->buttons[kButtonDpadRight].value = right ? 1.f : 0.f;
  mapped->buttons[kButtonDpadDown].pressed = down;
  mapped->buttons[kButtonDpadDown].value = down ? 1.f : 0.f;
  mapped->buttons[kButtonDpadLeft].pressed = left;
  mapped->buttons[kButtonDpadLeft].value = left ? 1.f : 0.f;
}

}  // namespace content
