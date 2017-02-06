// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_win.h"

#include <stddef.h>
#include <string.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"

namespace device {

using namespace blink;

namespace {

// See http://goo.gl/5VSJR. These are not available in all versions of the
// header, but they can be returned from the driver, so we define our own
// versions here.
static const BYTE kDeviceSubTypeGamepad = 1;
static const BYTE kDeviceSubTypeWheel = 2;
static const BYTE kDeviceSubTypeArcadeStick = 3;
static const BYTE kDeviceSubTypeFlightStick = 4;
static const BYTE kDeviceSubTypeDancePad = 5;
static const BYTE kDeviceSubTypeGuitar = 6;
static const BYTE kDeviceSubTypeGuitarAlternate = 7;
static const BYTE kDeviceSubTypeDrumKit = 8;
static const BYTE kDeviceSubTypeGuitarBass = 11;
static const BYTE kDeviceSubTypeArcadePad = 19;

float NormalizeXInputAxis(SHORT value) {
  return ((value + 32768.f) / 32767.5f) - 1.f;
}

const WebUChar* GamepadSubTypeName(BYTE sub_type) {
  switch (sub_type) {
    case kDeviceSubTypeGamepad:
      return L"GAMEPAD";
    case kDeviceSubTypeWheel:
      return L"WHEEL";
    case kDeviceSubTypeArcadeStick:
      return L"ARCADE_STICK";
    case kDeviceSubTypeFlightStick:
      return L"FLIGHT_STICK";
    case kDeviceSubTypeDancePad:
      return L"DANCE_PAD";
    case kDeviceSubTypeGuitar:
      return L"GUITAR";
    case kDeviceSubTypeGuitarAlternate:
      return L"GUITAR_ALTERNATE";
    case kDeviceSubTypeDrumKit:
      return L"DRUM_KIT";
    case kDeviceSubTypeGuitarBass:
      return L"GUITAR_BASS";
    case kDeviceSubTypeArcadePad:
      return L"ARCADE_PAD";
    default:
      return L"<UNKNOWN>";
  }
}

const WebUChar* XInputDllFileName() {
  // Xinput.h defines filename (XINPUT_DLL) on different Windows versions, but
  // Xinput.h specifies it in build time. Approach here uses the same values
  // and it is resolving dll filename based on Windows version it is running on.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    // For Windows 8 and 10, XINPUT_DLL is xinput1_4.dll.
    return FILE_PATH_LITERAL("xinput1_4.dll");
  } else if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    return FILE_PATH_LITERAL("xinput9_1_0.dll");
  } else {
    NOTREACHED();
    return nullptr;
  }
}

}  // namespace

GamepadPlatformDataFetcherWin::GamepadPlatformDataFetcherWin()
    : xinput_dll_(base::FilePath(XInputDllFileName())),
      xinput_available_(GetXInputDllFunctions()) {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    platform_pad_state_[i].status = DISCONNECTED;
    pad_state_[i].mapper = NULL;
    pad_state_[i].axis_mask = 0;
    pad_state_[i].button_mask = 0;
  }

  raw_input_fetcher_.reset(new RawInputDataFetcher());
  raw_input_fetcher_->StartMonitor();
}

GamepadPlatformDataFetcherWin::~GamepadPlatformDataFetcherWin() {
  raw_input_fetcher_->StopMonitor();
}

int GamepadPlatformDataFetcherWin::FirstAvailableGamepadId() const {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (platform_pad_state_[i].status == DISCONNECTED)
      return i;
  }
  return -1;
}

bool GamepadPlatformDataFetcherWin::HasXInputGamepad(int index) const {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (platform_pad_state_[i].status == XINPUT_CONNECTED &&
        platform_pad_state_[i].xinput_index == index)
      return true;
  }
  return false;
}

bool GamepadPlatformDataFetcherWin::HasRawInputGamepad(
    const HANDLE handle) const {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (platform_pad_state_[i].status == RAWINPUT_CONNECTED &&
        platform_pad_state_[i].raw_input_handle == handle)
      return true;
  }
  return false;
}

void GamepadPlatformDataFetcherWin::EnumerateDevices() {
  TRACE_EVENT0("GAMEPAD", "EnumerateDevices");

  // Mark all disconnected pads DISCONNECTED.
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (!pad_state_[i].data.connected)
      platform_pad_state_[i].status = DISCONNECTED;
  }

  for (size_t i = 0; i < XUSER_MAX_COUNT; ++i) {
    if (HasXInputGamepad(i))
      continue;
    int pad_index = FirstAvailableGamepadId();
    if (pad_index == -1)
      return;  // We can't add any more gamepads.
    WebGamepad& pad = pad_state_[pad_index].data;
    if (xinput_available_ && GetXInputPadConnectivity(i, &pad)) {
      platform_pad_state_[pad_index].status = XINPUT_CONNECTED;
      platform_pad_state_[pad_index].xinput_index = i;
      pad_state_[pad_index].mapper = NULL;
      pad_state_[pad_index].axis_mask = 0;
      pad_state_[pad_index].button_mask = 0;
    }
  }

  if (raw_input_fetcher_->Available()) {
    std::vector<RawGamepadInfo*> raw_inputs =
        raw_input_fetcher_->EnumerateDevices();
    for (size_t i = 0; i < raw_inputs.size(); ++i) {
      RawGamepadInfo* gamepad = raw_inputs[i];
      if (gamepad->buttons_length == 0 && gamepad->axes_length == 0)
        continue;
      if (HasRawInputGamepad(gamepad->handle))
        continue;
      int pad_index = FirstAvailableGamepadId();
      if (pad_index == -1)
        return;
      WebGamepad& pad = pad_state_[pad_index].data;
      pad.connected = true;
      PadState& state = pad_state_[pad_index];
      PlatformPadState& platform_state = platform_pad_state_[pad_index];
      platform_state.status = RAWINPUT_CONNECTED;
      platform_state.raw_input_handle = gamepad->handle;

      std::string vendor = base::StringPrintf("%04x", gamepad->vendor_id);
      std::string product = base::StringPrintf("%04x", gamepad->product_id);
      state.mapper = GetGamepadStandardMappingFunction(vendor, product);
      state.axis_mask = 0;
      state.button_mask = 0;

      swprintf(pad.id, WebGamepad::idLengthCap,
               L"%ls (%lsVendor: %04x Product: %04x)", gamepad->id,
               state.mapper ? L"STANDARD GAMEPAD " : L"", gamepad->vendor_id,
               gamepad->product_id);

      if (state.mapper)
        swprintf(pad.mapping, WebGamepad::mappingLengthCap, L"standard");
      else
        pad.mapping[0] = 0;
    }
  }
}

void GamepadPlatformDataFetcherWin::GetGamepadData(WebGamepads* pads,
                                                   bool devices_changed_hint) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  if (!xinput_available_ && !raw_input_fetcher_->Available()) {
    pads->length = 0;
    return;
  }

  // A note on XInput devices:
  // If we got notification that system devices have been updated, then
  // run GetCapabilities to update the connected status and the device
  // identifier. It can be slow to do to both GetCapabilities and
  // GetState on unconnected devices, so we want to avoid a 2-5ms pause
  // here by only doing this when the devices are updated (despite
  // documentation claiming it's OK to call it any time).
  if (devices_changed_hint)
    EnumerateDevices();

  pads->length = 0;

  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    // We rely on device_changed and GetCapabilities to tell us that
    // something's been connected, but we will mark as disconnected if
    // Get___PadState returns that we've lost the pad.
    if (!pad_state_[i].data.connected)
      continue;

    if (platform_pad_state_[i].status == XINPUT_CONNECTED)
      GetXInputPadData(i, &pad_state_[i].data);
    else if (platform_pad_state_[i].status == RAWINPUT_CONNECTED)
      GetRawInputPadData(i, &pad_state_[i].data);

    MapAndSanitizeGamepadData(&pad_state_[i], &pads->items[i]);

    if (pads->items[i].connected)
      pads->length++;
  }
}

void GamepadPlatformDataFetcherWin::PauseHint(bool pause) {
  if (pause)
    raw_input_fetcher_->StopMonitor();
  else
    raw_input_fetcher_->StartMonitor();
}

bool GamepadPlatformDataFetcherWin::GetXInputPadConnectivity(
    int i,
    WebGamepad* pad) const {
  DCHECK(pad);
  TRACE_EVENT1("GAMEPAD", "GetXInputPadConnectivity", "id", i);
  XINPUT_CAPABILITIES caps;
  DWORD res = xinput_get_capabilities_(i, XINPUT_FLAG_GAMEPAD, &caps);
  if (res == ERROR_DEVICE_NOT_CONNECTED) {
    pad->connected = false;
    return false;
  } else {
    pad->connected = true;
    swprintf(pad->id, WebGamepad::idLengthCap,
             L"Xbox 360 Controller (XInput STANDARD %ls)",
             GamepadSubTypeName(caps.SubType));
    swprintf(pad->mapping, WebGamepad::mappingLengthCap, L"standard");
    return true;
  }
}

void GamepadPlatformDataFetcherWin::GetXInputPadData(int i, WebGamepad* pad) {
  XINPUT_STATE state;
  memset(&state, 0, sizeof(XINPUT_STATE));
  TRACE_EVENT_BEGIN1("GAMEPAD", "XInputGetState", "id", i);
  DWORD dwResult =
      xinput_get_state_(platform_pad_state_[i].xinput_index, &state);
  TRACE_EVENT_END1("GAMEPAD", "XInputGetState", "id", i);

  if (dwResult == ERROR_SUCCESS) {
    pad->timestamp = state.dwPacketNumber;
    pad->buttonsLength = 0;
    WORD val = state.Gamepad.wButtons;
#define ADD(b)                                                 \
  pad->buttons[pad->buttonsLength].pressed = (val & (b)) != 0; \
  pad->buttons[pad->buttonsLength++].value = ((val & (b)) ? 1.f : 0.f);
    ADD(XINPUT_GAMEPAD_A);
    ADD(XINPUT_GAMEPAD_B);
    ADD(XINPUT_GAMEPAD_X);
    ADD(XINPUT_GAMEPAD_Y);
    ADD(XINPUT_GAMEPAD_LEFT_SHOULDER);
    ADD(XINPUT_GAMEPAD_RIGHT_SHOULDER);

    pad->buttons[pad->buttonsLength].pressed =
        state.Gamepad.bLeftTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    pad->buttons[pad->buttonsLength++].value =
        state.Gamepad.bLeftTrigger / 255.f;

    pad->buttons[pad->buttonsLength].pressed =
        state.Gamepad.bRightTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    pad->buttons[pad->buttonsLength++].value =
        state.Gamepad.bRightTrigger / 255.f;

    ADD(XINPUT_GAMEPAD_BACK);
    ADD(XINPUT_GAMEPAD_START);
    ADD(XINPUT_GAMEPAD_LEFT_THUMB);
    ADD(XINPUT_GAMEPAD_RIGHT_THUMB);
    ADD(XINPUT_GAMEPAD_DPAD_UP);
    ADD(XINPUT_GAMEPAD_DPAD_DOWN);
    ADD(XINPUT_GAMEPAD_DPAD_LEFT);
    ADD(XINPUT_GAMEPAD_DPAD_RIGHT);
#undef ADD
    pad->axesLength = 0;

    float value = 0.0;
#define ADD(a, factor)                     \
  value = factor * NormalizeXInputAxis(a); \
  pad->axes[pad->axesLength++] = value;

    // XInput are +up/+right, -down/-left, we want -up/-left.
    ADD(state.Gamepad.sThumbLX, 1);
    ADD(state.Gamepad.sThumbLY, -1);
    ADD(state.Gamepad.sThumbRX, 1);
    ADD(state.Gamepad.sThumbRY, -1);
#undef ADD
  } else {
    pad->connected = false;
  }
}

void GamepadPlatformDataFetcherWin::GetRawInputPadData(int index,
                                                       WebGamepad* pad) {
  RawGamepadInfo* gamepad = raw_input_fetcher_->GetGamepadInfo(
      platform_pad_state_[index].raw_input_handle);
  if (!gamepad) {
    pad->connected = false;
    return;
  }

  pad->timestamp = gamepad->report_id;
  pad->buttonsLength = gamepad->buttons_length;
  pad->axesLength = gamepad->axes_length;

  for (unsigned int i = 0; i < pad->buttonsLength; i++) {
    pad->buttons[i].pressed = gamepad->buttons[i];
    pad->buttons[i].value = gamepad->buttons[i] ? 1.0 : 0.0;
  }

  for (unsigned int i = 0; i < pad->axesLength; i++)
    pad->axes[i] = gamepad->axes[i].value;
}

bool GamepadPlatformDataFetcherWin::GetXInputDllFunctions() {
  xinput_get_capabilities_ = NULL;
  xinput_get_state_ = NULL;
  XInputEnableFunc xinput_enable = reinterpret_cast<XInputEnableFunc>(
      xinput_dll_.GetFunctionPointer("XInputEnable"));
  xinput_get_capabilities_ = reinterpret_cast<XInputGetCapabilitiesFunc>(
      xinput_dll_.GetFunctionPointer("XInputGetCapabilities"));
  if (!xinput_get_capabilities_)
    return false;
  xinput_get_state_ = reinterpret_cast<XInputGetStateFunc>(
      xinput_dll_.GetFunctionPointer("XInputGetState"));
  if (!xinput_get_state_)
    return false;
  if (xinput_enable) {
    // XInputEnable is unavailable before Win8 and deprecated in Win10.
    xinput_enable(true);
  }
  return true;
}

}  // namespace device
