// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {

void MapperXbox360Gamepad(const blink::WebGamepad& input,
                          blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  mapped->buttonsLength = BUTTON_INDEX_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperXboxOneHidGamepad(const blink::WebGamepad& input,
                             blink::WebGamepad* mapped) {
  *mapped = input;

  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperPlaystationSixAxis(const blink::WebGamepad& input,
                              blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[11];

  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] =
      ButtonFromButtonAndAxis(input.buttons[8], input.axes[14]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] =
      ButtonFromButtonAndAxis(input.buttons[9], input.axes[15]);

  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[2];

  // The SixAxis Dpad is pressure sensitive.
  mapped->buttons[BUTTON_INDEX_DPAD_UP] =
      ButtonFromButtonAndAxis(input.buttons[4], input.axes[10]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] =
      ButtonFromButtonAndAxis(input.buttons[6], input.axes[12]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] =
      ButtonFromButtonAndAxis(input.buttons[7], input.axes[13]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      ButtonFromButtonAndAxis(input.buttons[5], input.axes[11]);

  mapped->buttons[BUTTON_INDEX_META] = input.buttons[16];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttonsLength = BUTTON_INDEX_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperDualshock4(const blink::WebGamepad& input,
                      blink::WebGamepad* mapped) {
  enum Dualshock4Buttons {
    DUALSHOCK_BUTTON_TOUCHPAD = BUTTON_INDEX_COUNT,
    DUALSHOCK_BUTTON_COUNT
  };

  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[DUALSHOCK_BUTTON_TOUCHPAD] = input.buttons[13];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = DUALSHOCK_BUTTON_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperIBuffalo(const blink::WebGamepad& input, blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[1]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[1]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[0]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[0]);
  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = 2;
}

void MapperDirectInputStyle(const blink::WebGamepad& input,
                            blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperMacallyIShock(const blink::WebGamepad& input,
                         blink::WebGamepad* mapped) {
  enum IShockButtons {
    ISHOCK_BUTTON_C = BUTTON_INDEX_COUNT,
    ISHOCK_BUTTON_D,
    ISHOCK_BUTTON_E,
    ISHOCK_BUTTON_COUNT,
  };

  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[17];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[11];
  mapped->buttons[ISHOCK_BUTTON_C] = input.buttons[8];
  mapped->buttons[ISHOCK_BUTTON_D] = input.buttons[18];
  mapped->buttons[ISHOCK_BUTTON_E] = input.buttons[19];
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = -input.axes[5];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[6];

  mapped->buttonsLength = ISHOCK_BUTTON_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperXGEAR(const blink::WebGamepad& input, blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[5];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[2];
  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperSmartJoyPLUS(const blink::WebGamepad& input,
                        blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperDragonRiseGeneric(const blink::WebGamepad& input,
                             blink::WebGamepad* mapped) {
  *mapped = input;
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperOnLiveWireless(const blink::WebGamepad& input,
                          blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = BUTTON_INDEX_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperADT1(const blink::WebGamepad& input, blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = BUTTON_INDEX_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperNvShield(const blink::WebGamepad& input, blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[8];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = BUTTON_INDEX_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperOUYA(const blink::WebGamepad& input, blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[15];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttonsLength = BUTTON_INDEX_COUNT;
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperRazerServal(const blink::WebGamepad& input,
                       blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = AXIS_INDEX_COUNT;
}

void MapperMogaPro(const blink::WebGamepad& input, blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axesLength = AXIS_INDEX_COUNT;
}

struct MappingData {
  const char* const vendor_id;
  const char* const product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
    // http://www.linux-usb.org/usb.ids
    {"0079", "0006", MapperDragonRiseGeneric},   // DragonRise Generic USB
    {"045e", "028e", MapperXbox360Gamepad},      // Xbox 360 Wired
    {"045e", "028f", MapperXbox360Gamepad},      // Xbox 360 Wireless
    {"045e", "02d1", MapperXbox360Gamepad},      // Xbox One Wired
    {"045e", "02dd", MapperXbox360Gamepad},      // Xbox One Wired (2015 FW)
    {"045e", "02e3", MapperXbox360Gamepad},      // Xbox One Elite Wired
    {"045e", "02ea", MapperXbox360Gamepad},      // Xbox One S (USB)
    {"045e", "02fd", MapperXboxOneHidGamepad},   // Xbox One S (Bluetooth)
    {"045e", "0719", MapperXbox360Gamepad},      // Xbox 360 Wireless
    {"046d", "c216", MapperDirectInputStyle},    // Logitech F310, D mode
    {"046d", "c218", MapperDirectInputStyle},    // Logitech F510, D mode
    {"046d", "c219", MapperDirectInputStyle},    // Logitech F710, D mode
    {"054c", "0268", MapperPlaystationSixAxis},  // Playstation SIXAXIS
    {"054c", "05c4", MapperDualshock4},          // Playstation Dualshock 4
    {"054c", "09cc", MapperDualshock4},          // Dualshock 4 (PS4 Slim)
    {"0583", "2060", MapperIBuffalo},            // iBuffalo Classic
    {"0925", "0005", MapperSmartJoyPLUS},        // SmartJoy PLUS Adapter
    {"0955", "7210", MapperNvShield},            // Nvidia Shield gamepad
    {"0b05", "4500", MapperADT1},                // Nexus Player Controller
    {"0e8f", "0003", MapperXGEAR},             // XFXforce XGEAR PS2 Controller
    {"1532", "0900", MapperRazerServal},       // Razer Serval Controller
    {"18d1", "2c40", MapperADT1},              // ADT-1 Controller
    {"20d6", "6271", MapperMogaPro},           // Moga Pro Controller (HID mode)
    {"2222", "0060", MapperDirectInputStyle},  // Macally iShockX, analog mode
    {"2222", "4010", MapperMacallyIShock},     // Macally iShock
    {"2378", "1008", MapperOnLiveWireless},    // OnLive Controller (Bluetooth)
    {"2378", "100a", MapperOnLiveWireless},    // OnLive Controller (Wired)
    {"2836", "0001", MapperOUYA},              // OUYA Controller
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

}  // namespace device
