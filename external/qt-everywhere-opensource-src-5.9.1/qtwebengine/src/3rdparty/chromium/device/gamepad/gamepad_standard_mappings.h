// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_
#define DEVICE_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_

#include "base/strings/string_piece.h"
#include "third_party/WebKit/public/platform/WebGamepad.h"

namespace device {

typedef void (*GamepadStandardMappingFunction)(
    const blink::WebGamepad& original,
    blink::WebGamepad* mapped);

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const base::StringPiece& vendor_id,
    const base::StringPiece& product_id);

// This defines our canonical mapping order for gamepad-like devices. If these
// items cannot all be satisfied, it is a case-by-case judgement as to whether
// it is better to leave the device unmapped, or to partially map it. In
// general, err towards leaving it *unmapped* so that content can handle
// appropriately.

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.gamepad
// GENERATED_JAVA_PREFIX_TO_STRIP: BUTTON_INDEX_
enum CanonicalButtonIndex {
  BUTTON_INDEX_PRIMARY,
  BUTTON_INDEX_SECONDARY,
  BUTTON_INDEX_TERTIARY,
  BUTTON_INDEX_QUATERNARY,
  BUTTON_INDEX_LEFT_SHOULDER,
  BUTTON_INDEX_RIGHT_SHOULDER,
  BUTTON_INDEX_LEFT_TRIGGER,
  BUTTON_INDEX_RIGHT_TRIGGER,
  BUTTON_INDEX_BACK_SELECT,
  BUTTON_INDEX_START,
  BUTTON_INDEX_LEFT_THUMBSTICK,
  BUTTON_INDEX_RIGHT_THUMBSTICK,
  BUTTON_INDEX_DPAD_UP,
  BUTTON_INDEX_DPAD_DOWN,
  BUTTON_INDEX_DPAD_LEFT,
  BUTTON_INDEX_DPAD_RIGHT,
  BUTTON_INDEX_META,
  BUTTON_INDEX_COUNT
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.gamepad
// GENERATED_JAVA_PREFIX_TO_STRIP: AXIS_INDEX_
enum CanonicalAxisIndex {
  AXIS_INDEX_LEFT_STICK_X,
  AXIS_INDEX_LEFT_STICK_Y,
  AXIS_INDEX_RIGHT_STICK_X,
  AXIS_INDEX_RIGHT_STICK_Y,
  AXIS_INDEX_COUNT
};

// Matches XInput's trigger deadzone
const float kDefaultButtonPressedThreshold = 30.f / 255.f;

// Common mapping functions
blink::WebGamepadButton AxisToButton(float input);
blink::WebGamepadButton AxisNegativeAsButton(float input);
blink::WebGamepadButton AxisPositiveAsButton(float input);
blink::WebGamepadButton ButtonFromButtonAndAxis(blink::WebGamepadButton button,
                                                float axis);
blink::WebGamepadButton NullButton();
void DpadFromAxis(blink::WebGamepad* mapped, float dir);

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_
