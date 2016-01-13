// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_standard_mappings.h"

namespace content {

namespace {

void MapperXInputStyleGamepad(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[2]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[5]);
  mapped->buttons[kButtonBackSelect] = input.buttons[6];
  mapped->buttons[kButtonStart] = input.buttons[7];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[9];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[10];
  mapped->buttons[kButtonDpadUp] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[kButtonDpadDown] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[kButtonDpadLeft] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[kButtonDpadRight] = AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[kButtonMeta] = input.buttons[8];
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[4];
  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}

void MapperLakeviewResearch(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[2];
  mapped->buttons[kButtonTertiary] = input.buttons[3];
  mapped->buttons[kButtonQuaternary] = input.buttons[0];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[6];
  mapped->buttons[kButtonRightShoulder] = input.buttons[7];
  mapped->buttons[kButtonLeftTrigger] = input.buttons[4];
  mapped->buttons[kButtonRightTrigger] = input.buttons[5];
  mapped->buttons[kButtonBackSelect] = input.buttons[9];
  mapped->buttons[kButtonStart] = input.buttons[8];
  mapped->buttons[kButtonDpadUp] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[kButtonDpadDown] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[kButtonDpadLeft] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[kButtonDpadRight] = AxisPositiveAsButton(input.axes[4]);
  mapped->buttonsLength = kNumButtons - 1; // no Meta on this device
  mapped->axesLength = kNumAxes;
}

void MapperPlaystationSixAxis(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[14];
  mapped->buttons[kButtonSecondary] = input.buttons[13];
  mapped->buttons[kButtonTertiary] = input.buttons[15];
  mapped->buttons[kButtonQuaternary] = input.buttons[12];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[10];
  mapped->buttons[kButtonRightShoulder] = input.buttons[11];
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[12]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[13]);
  mapped->buttons[kButtonBackSelect] = input.buttons[0];
  mapped->buttons[kButtonStart] = input.buttons[3];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[1];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[2];
  mapped->buttons[kButtonDpadUp] = AxisToButton(input.axes[8]);
  mapped->buttons[kButtonDpadDown] = AxisToButton(input.axes[10]);
  mapped->buttons[kButtonDpadLeft] = input.buttons[7];
  mapped->buttons[kButtonDpadRight] = AxisToButton(input.axes[9]);
  mapped->buttons[kButtonMeta] = input.buttons[16];

  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}

void MapperDualshock4(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  enum Dualshock4Buttons {
    kTouchpadButton = kNumButtons,
    kNumDualshock4Buttons
  };

  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[1];
  mapped->buttons[kButtonSecondary] = input.buttons[2];
  mapped->buttons[kButtonTertiary] = input.buttons[0];
  mapped->buttons[kButtonQuaternary] = input.buttons[3];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[4];
  mapped->buttons[kButtonRightShoulder] = input.buttons[5];
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[3]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[4]);
  mapped->buttons[kButtonBackSelect] = input.buttons[8];
  mapped->buttons[kButtonStart] = input.buttons[9];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[10];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[11];
  mapped->buttons[kButtonDpadUp] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[kButtonDpadDown] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[kButtonDpadLeft] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[kButtonDpadRight] = AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[kButtonMeta] = input.buttons[12];
  mapped->buttons[kTouchpadButton] = input.buttons[13];
  mapped->axes[kAxisRightStickY] = input.axes[5];

  mapped->buttonsLength = kNumDualshock4Buttons;
  mapped->axesLength = kNumAxes;
}

void MapperXGEAR(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[2];
  mapped->buttons[kButtonSecondary] = input.buttons[1];
  mapped->buttons[kButtonTertiary] = input.buttons[3];
  mapped->buttons[kButtonQuaternary] = input.buttons[0];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[6];
  mapped->buttons[kButtonRightShoulder] = input.buttons[7];
  mapped->buttons[kButtonLeftTrigger] = input.buttons[4];
  mapped->buttons[kButtonRightTrigger] = input.buttons[5];
  mapped->buttons[kButtonDpadUp] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[kButtonDpadDown] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[kButtonDpadLeft] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[kButtonDpadRight] = AxisPositiveAsButton(input.axes[4]);
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[2];
  mapped->buttonsLength = kNumButtons - 1; // no Meta on this device
  mapped->axesLength = kNumAxes;
}


void MapperDragonRiseGeneric(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonDpadUp] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[kButtonDpadDown] = AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[kButtonDpadLeft] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[kButtonDpadRight] = AxisPositiveAsButton(input.axes[5]);
  mapped->axes[kAxisLeftStickX] = input.axes[0];
  mapped->axes[kAxisLeftStickY] = input.axes[1];
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[4];
  mapped->buttonsLength = kNumButtons - 1; // no Meta on this device
  mapped->axesLength = kNumAxes;
}

void MapperOnLiveWireless(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[2]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[5]);
  mapped->buttons[kButtonBackSelect] = input.buttons[6];
  mapped->buttons[kButtonStart] = input.buttons[7];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[9];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[10];
  mapped->buttons[kButtonDpadUp] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[kButtonDpadDown] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[kButtonDpadLeft] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[kButtonDpadRight] = AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[kButtonMeta] = input.buttons[8];
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[4];

  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}


struct MappingData {
  const char* const vendor_id;
  const char* const product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
  // http://www.linux-usb.org/usb.ids
  { "0079", "0006", MapperDragonRiseGeneric }, // DragonRise Generic USB
  { "045e", "028e", MapperXInputStyleGamepad }, // Xbox 360 Controller
  { "045e", "028f", MapperXInputStyleGamepad }, // Xbox 360 Wireless Controller
  { "046d", "c21d", MapperXInputStyleGamepad }, // Logitech F310
  { "046d", "c21e", MapperXInputStyleGamepad }, // Logitech F510
  { "046d", "c21f", MapperXInputStyleGamepad }, // Logitech F710
  { "054c", "0268", MapperPlaystationSixAxis }, // Playstation SIXAXIS
  { "054c", "05c4", MapperDualshock4 },         // Playstation Dualshock 4
  { "0925", "0005", MapperLakeviewResearch }, // SmartJoy PLUS Adapter
  { "0925", "8866", MapperLakeviewResearch }, // WiseGroup MP-8866
  { "0e8f", "0003", MapperXGEAR }, // XFXforce XGEAR PS2 Controller
  { "2378", "1008", MapperOnLiveWireless }, // OnLive Controller (Bluetooth)
  { "2378", "100a", MapperOnLiveWireless }, // OnLive Controller (Wired)
};

}  // namespace

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const base::StringPiece& vendor_id,
    const base::StringPiece& product_id) {
  for (size_t i = 0; i < arraysize(AvailableMappings); ++i) {
    MappingData& item = AvailableMappings[i];
    if (vendor_id == item.vendor_id && product_id == item.product_id)
      return item.function;
  }
  return NULL;
}

}  // namespace content
