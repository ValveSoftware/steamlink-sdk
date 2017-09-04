// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_

#include <Unknwn.h>
#include <WinDef.h>
#include <hidsdi.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_native_library.h"
#include "base/win/message_window.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace device {

struct RawGamepadAxis {
  HIDP_VALUE_CAPS caps;
  float value;
  bool active;
  unsigned long bitmask;
};

struct RawGamepadInfo {
  RawGamepadInfo();
  ~RawGamepadInfo();

  int source_id;
  int enumeration_id;
  HANDLE handle;
  std::unique_ptr<uint8_t[]> ppd_buffer;
  PHIDP_PREPARSED_DATA preparsed_data;

  uint32_t report_id;
  uint32_t vendor_id;
  uint32_t product_id;

  wchar_t id[blink::WebGamepad::idLengthCap];

  uint32_t buttons_length;
  bool buttons[blink::WebGamepad::buttonsLengthCap];

  uint32_t axes_length;
  RawGamepadAxis axes[blink::WebGamepad::axesLengthCap];
};

class RawInputDataFetcher : public GamepadDataFetcher,
                            public base::SupportsWeakPtr<RawInputDataFetcher>,
                            public base::MessageLoop::DestructionObserver {
 public:
  typedef GamepadDataFetcherFactoryImpl<RawInputDataFetcher,
                                        GAMEPAD_SOURCE_WIN_RAW>
      Factory;

  explicit RawInputDataFetcher();
  ~RawInputDataFetcher() override;

  GamepadSource source() override;

  // DestructionObserver overrides.
  void WillDestroyCurrentMessageLoop() override;

  void GetGamepadData(bool devices_changed_hint) override;
  void PauseHint(bool paused) override;

 private:
  void OnAddedToProvider() override;

  void StartMonitor();
  void StopMonitor();
  void EnumerateDevices();
  RawGamepadInfo* ParseGamepadInfo(HANDLE hDevice);
  void UpdateGamepad(RAWINPUT* input, RawGamepadInfo* gamepad_info);
  // Handles WM_INPUT messages.
  LRESULT OnInput(HRAWINPUT input_handle);
  // Handles messages received by |window_|.
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);
  RAWINPUTDEVICE* GetRawInputDevices(DWORD flags);
  void ClearControllers();

  // Function types we use from hid.dll.
  typedef NTSTATUS(__stdcall* HidPGetCapsFunc)(
      PHIDP_PREPARSED_DATA PreparsedData,
      PHIDP_CAPS Capabilities);
  typedef NTSTATUS(__stdcall* HidPGetButtonCapsFunc)(
      HIDP_REPORT_TYPE ReportType,
      PHIDP_BUTTON_CAPS ButtonCaps,
      PUSHORT ButtonCapsLength,
      PHIDP_PREPARSED_DATA PreparsedData);
  typedef NTSTATUS(__stdcall* HidPGetValueCapsFunc)(
      HIDP_REPORT_TYPE ReportType,
      PHIDP_VALUE_CAPS ValueCaps,
      PUSHORT ValueCapsLength,
      PHIDP_PREPARSED_DATA PreparsedData);
  typedef NTSTATUS(__stdcall* HidPGetUsagesExFunc)(
      HIDP_REPORT_TYPE ReportType,
      USHORT LinkCollection,
      PUSAGE_AND_PAGE ButtonList,
      ULONG* UsageLength,
      PHIDP_PREPARSED_DATA PreparsedData,
      PCHAR Report,
      ULONG ReportLength);
  typedef NTSTATUS(__stdcall* HidPGetUsageValueFunc)(
      HIDP_REPORT_TYPE ReportType,
      USAGE UsagePage,
      USHORT LinkCollection,
      USAGE Usage,
      PULONG UsageValue,
      PHIDP_PREPARSED_DATA PreparsedData,
      PCHAR Report,
      ULONG ReportLength);
  typedef NTSTATUS(__stdcall* HidPGetScaledUsageValueFunc)(
      HIDP_REPORT_TYPE ReportType,
      USAGE UsagePage,
      USHORT LinkCollection,
      USAGE Usage,
      PLONG UsageValue,
      PHIDP_PREPARSED_DATA PreparsedData,
      PCHAR Report,
      ULONG ReportLength);
  typedef BOOLEAN(__stdcall* HidDGetStringFunc)(HANDLE HidDeviceObject,
                                                PVOID Buffer,
                                                ULONG BufferLength);

  // Get functions from dynamically loaded hid.dll. Returns true if loading was
  // successful.
  bool GetHidDllFunctions();

  base::ScopedNativeLibrary hid_dll_;
  std::unique_ptr<base::win::MessageWindow> window_;
  bool rawinput_available_;
  bool filter_xinput_;
  bool events_monitored_;
  int last_source_id_;
  int last_enumeration_id_;

  typedef std::map<HANDLE, RawGamepadInfo*> ControllerMap;
  ControllerMap controllers_;

  // Function pointers to HID functionality, retrieved in
  // |GetHidDllFunctions|.
  HidPGetCapsFunc hidp_get_caps_;
  HidPGetButtonCapsFunc hidp_get_button_caps_;
  HidPGetValueCapsFunc hidp_get_value_caps_;
  HidPGetUsagesExFunc hidp_get_usages_ex_;
  HidPGetUsageValueFunc hidp_get_usage_value_;
  HidPGetScaledUsageValueFunc hidp_get_scaled_usage_value_;
  HidDGetStringFunc hidd_get_product_string_;

  DISALLOW_COPY_AND_ASSIGN(RawInputDataFetcher);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
