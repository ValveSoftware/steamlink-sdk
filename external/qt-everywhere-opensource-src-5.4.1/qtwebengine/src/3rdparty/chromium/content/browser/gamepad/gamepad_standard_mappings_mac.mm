// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_standard_mappings.h"

namespace content {

namespace {

void MapperXbox360Gamepad(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[2]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[5]);
  mapped->buttons[kButtonBackSelect] = input.buttons[9];
  mapped->buttons[kButtonStart] = input.buttons[8];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[6];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[7];
  mapped->buttons[kButtonDpadUp] = input.buttons[11];
  mapped->buttons[kButtonDpadDown] = input.buttons[12];
  mapped->buttons[kButtonDpadLeft] = input.buttons[13];
  mapped->buttons[kButtonDpadRight] = input.buttons[14];
  mapped->buttons[kButtonMeta] = input.buttons[10];
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[4];
  mapped->buttonsLength = kNumButtons;
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

  mapped->buttons[kButtonLeftTrigger] = ButtonFromButtonAndAxis(
      input.buttons[8], input.axes[14]);
  mapped->buttons[kButtonRightTrigger] = ButtonFromButtonAndAxis(
      input.buttons[9], input.axes[15]);

  mapped->buttons[kButtonBackSelect] = input.buttons[0];
  mapped->buttons[kButtonStart] = input.buttons[3];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[1];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[2];

  // The SixAxis Dpad is pressure sensative
  mapped->buttons[kButtonDpadUp] = ButtonFromButtonAndAxis(
      input.buttons[4], input.axes[10]);
  mapped->buttons[kButtonDpadDown] = ButtonFromButtonAndAxis(
      input.buttons[6], input.axes[12]);
  mapped->buttons[kButtonDpadLeft] = ButtonFromButtonAndAxis(
      input.buttons[7], input.axes[13]);
  mapped->buttons[kButtonDpadRight] = ButtonFromButtonAndAxis(
      input.buttons[5], input.axes[11]);

  mapped->buttons[kButtonMeta] = input.buttons[16];
  mapped->axes[kAxisRightStickY] = input.axes[5];

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
  mapped->buttons[kButtonMeta] = input.buttons[12];
  mapped->buttons[kTouchpadButton] = input.buttons[13];
  mapped->axes[kAxisRightStickY] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = kNumDualshock4Buttons;
  mapped->axesLength = kNumAxes;
}

void MapperDirectInputStyle(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[1];
  mapped->buttons[kButtonSecondary] = input.buttons[2];
  mapped->buttons[kButtonTertiary] = input.buttons[0];
  mapped->axes[kAxisRightStickY] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->buttonsLength = kNumButtons - 1; /* no meta */
  mapped->axesLength = kNumAxes;
}

void MapperMacallyIShock(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  enum IShockButtons {
    kButtonC = kNumButtons,
    kButtonD,
    kButtonE,
    kNumIShockButtons
  };

  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[6];
  mapped->buttons[kButtonSecondary] = input.buttons[5];
  mapped->buttons[kButtonTertiary] = input.buttons[7];
  mapped->buttons[kButtonQuaternary] = input.buttons[4];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[14];
  mapped->buttons[kButtonRightShoulder] = input.buttons[12];
  mapped->buttons[kButtonLeftTrigger] = input.buttons[15];
  mapped->buttons[kButtonRightTrigger] = input.buttons[13];
  mapped->buttons[kButtonBackSelect] = input.buttons[9];
  mapped->buttons[kButtonStart] = input.buttons[10];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[16];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[17];
  mapped->buttons[kButtonDpadUp] = input.buttons[0];
  mapped->buttons[kButtonDpadDown] = input.buttons[1];
  mapped->buttons[kButtonDpadLeft] = input.buttons[2];
  mapped->buttons[kButtonDpadRight] = input.buttons[3];
  mapped->buttons[kButtonMeta] = input.buttons[11];
  mapped->buttons[kButtonC] = input.buttons[8];
  mapped->buttons[kButtonD] = input.buttons[18];
  mapped->buttons[kButtonE] = input.buttons[19];
  mapped->axes[kAxisLeftStickX] = input.axes[0];
  mapped->axes[kAxisLeftStickY] = input.axes[1];
  mapped->axes[kAxisRightStickX] = -input.axes[5];
  mapped->axes[kAxisRightStickY] = input.axes[6];

  mapped->buttonsLength = kNumIShockButtons;
  mapped->axesLength = kNumAxes;
}

void MapperXGEAR(
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
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[kAxisRightStickX] = input.axes[5];
  mapped->axes[kAxisRightStickY] = input.axes[2];
  mapped->buttonsLength = kNumButtons - 1; /* no meta */
  mapped->axesLength = kNumAxes;
}

void MapperSmartJoyPLUS(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[2];
  mapped->buttons[kButtonTertiary] = input.buttons[3];
  mapped->buttons[kButtonQuaternary] = input.buttons[0];
  mapped->buttons[kButtonStart] = input.buttons[8];
  mapped->buttons[kButtonBackSelect] = input.buttons[9];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[6];
  mapped->buttons[kButtonRightShoulder] = input.buttons[7];
  mapped->buttons[kButtonLeftTrigger] = input.buttons[4];
  mapped->buttons[kButtonRightTrigger] = input.buttons[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[kAxisRightStickY] = input.axes[5];
  mapped->buttonsLength = kNumButtons - 1; /* no meta */
  mapped->axesLength = kNumAxes;
}

void MapperDragonRiseGeneric(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[kAxisLeftStickX] = input.axes[0];
  mapped->axes[kAxisLeftStickY] = input.axes[1];
  mapped->axes[kAxisRightStickX] = input.axes[2];
  mapped->axes[kAxisRightStickY] = input.axes[5];
  mapped->buttonsLength = kNumButtons - 1; /* no meta */
  mapped->axesLength = kNumAxes;
}

void MapperOnLiveWireless(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[0];
  mapped->buttons[kButtonSecondary] = input.buttons[1];
  mapped->buttons[kButtonTertiary] = input.buttons[3];
  mapped->buttons[kButtonQuaternary] = input.buttons[4];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[6];
  mapped->buttons[kButtonRightShoulder] = input.buttons[7];
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[2]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[5]);
  mapped->buttons[kButtonBackSelect] = input.buttons[10];
  mapped->buttons[kButtonStart] = input.buttons[11];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[13];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[14];
  mapped->buttons[kButtonMeta] = input.buttons[12];
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[4];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}

struct MappingData {
  const char* const vendor_id;
  const char* const product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
  // http://www.linux-usb.org/usb.ids
  { "0079", "0006", MapperDragonRiseGeneric },  // DragonRise Generic USB
  { "045e", "028e", MapperXbox360Gamepad },     // Xbox 360 Controller
  { "045e", "028f", MapperXbox360Gamepad },     // Xbox 360 Wireless Controller
  { "046d", "c216", MapperDirectInputStyle },   // Logitech F310, D mode
  { "046d", "c218", MapperDirectInputStyle },   // Logitech F510, D mode
  { "046d", "c219", MapperDirectInputStyle },   // Logitech F710, D mode
  { "054c", "0268", MapperPlaystationSixAxis }, // Playstation SIXAXIS
  { "054c", "05c4", MapperDualshock4 },         // Playstation Dualshock 4
  { "0925", "0005", MapperSmartJoyPLUS },       // SmartJoy PLUS Adapter
  { "0e8f", "0003", MapperXGEAR },              // XFXforce XGEAR PS2 Controller
  { "2222", "0060", MapperDirectInputStyle },   // Macally iShockX, analog mode
  { "2222", "4010", MapperMacallyIShock },      // Macally iShock
  { "2378", "1008", MapperOnLiveWireless },     // OnLive Controller (Bluetooth)
  { "2378", "100a", MapperOnLiveWireless },     // OnLive Controller (Wired)
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
