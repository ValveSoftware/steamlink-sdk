// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_data_fetcher.h"

#include <stddef.h>
#include <string.h>

#include <cmath>

#include "base/logging.h"
#include "build/build_config.h"

namespace {

#if !defined(OS_ANDROID)
const float kMinAxisResetValue = 0.1f;
#endif

}  // namespace

namespace device {

using blink::WebGamepad;
using blink::WebGamepads;

#if !defined(OS_ANDROID)
void GamepadDataFetcher::MapAndSanitizeGamepadData(PadState* pad_state,
                                                   WebGamepad* pad) {
  DCHECK(pad_state);
  DCHECK(pad);

  if (!pad_state->data.connected) {
    memset(pad, 0, sizeof(WebGamepad));
    return;
  }

  // Copy the current state to the output buffer, using the mapping
  // function, if there is one available.
  if (pad_state->mapper)
    pad_state->mapper(pad_state->data, pad);
  else
    *pad = pad_state->data;

  // About sanitization: Gamepads may report input event if the user is not
  // interacting with it, due to hardware problems or environmental ones (pad
  // has something heavy leaning against an axis.) This may cause user gestures
  // to be detected erroniously, exposing gamepad information when the user had
  // no intention of doing so. To avoid this we require that each button or axis
  // report being at rest (zero) at least once before exposing its value to the
  // Gamepad API. This state is tracked by the axis_mask and button_mask
  // bitfields. If the bit for an axis or button is 0 it means the axis has
  // never reported being at rest, and the value will be forced to zero.

  // We can skip axis sanitation if all available axes have been masked.
  uint32_t full_axis_mask = (1 << pad->axesLength) - 1;
  if (pad_state->axis_mask != full_axis_mask) {
    for (size_t axis = 0; axis < pad->axesLength; ++axis) {
      if (!(pad_state->axis_mask & 1 << axis)) {
        if (fabs(pad->axes[axis]) < kMinAxisResetValue) {
          pad_state->axis_mask |= 1 << axis;
        } else {
          pad->axes[axis] = 0.0f;
        }
      }
    }
  }

  // We can skip button sanitation if all available buttons have been masked.
  uint32_t full_button_mask = (1 << pad->buttonsLength) - 1;
  if (pad_state->button_mask != full_button_mask) {
    for (size_t button = 0; button < pad->buttonsLength; ++button) {
      if (!(pad_state->button_mask & 1 << button)) {
        if (!pad->buttons[button].pressed) {
          pad_state->button_mask |= 1 << button;
        } else {
          pad->buttons[button].pressed = false;
          pad->buttons[button].value = 0.0f;
        }
      }
    }
  }
}
#endif

}  // namespace device
