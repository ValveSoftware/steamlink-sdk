// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
#define CONTENT_BROWSER_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_

#include "build/build_config.h"

#include <stdlib.h>
#include <Unknwn.h>
#include <WinDef.h>
#include <windows.h>

#include <hidsdi.h>
#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_native_library.h"
#include "base/win/message_window.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

struct RawGamepadAxis {
  HIDP_VALUE_CAPS caps;
  float value;
  bool active;
};

struct RawGamepadInfo {
  HANDLE handle;
  scoped_ptr<uint8[]> ppd_buffer;
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

class RawInputDataFetcher
    : public base::SupportsWeakPtr<RawInputDataFetcher>,
      public base::MessageLoop::DestructionObserver {
 public:
  explicit RawInputDataFetcher();
  ~RawInputDataFetcher();

  // DestructionObserver overrides.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  bool Available() { return rawinput_available_; }
  void StartMonitor();
  void StopMonitor();

  std::vector<RawGamepadInfo*> EnumerateDevices();
  RawGamepadInfo* GetGamepadInfo(HANDLE handle);

 private:
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
  typedef NTSTATUS (__stdcall *HidPGetCapsFunc)(
      PHIDP_PREPARSED_DATA PreparsedData, PHIDP_CAPS Capabilities);
  typedef NTSTATUS (__stdcall *HidPGetButtonCapsFunc)(
      HIDP_REPORT_TYPE ReportType, PHIDP_BUTTON_CAPS ButtonCaps,
      PUSHORT ButtonCapsLength, PHIDP_PREPARSED_DATA PreparsedData);
  typedef NTSTATUS (__stdcall *HidPGetValueCapsFunc)(
      HIDP_REPORT_TYPE ReportType, PHIDP_VALUE_CAPS ValueCaps,
      PUSHORT ValueCapsLength, PHIDP_PREPARSED_DATA PreparsedData);
  typedef NTSTATUS(__stdcall* HidPGetUsagesExFunc)(
      HIDP_REPORT_TYPE ReportType,
      USHORT LinkCollection,
      PUSAGE_AND_PAGE ButtonList,
      ULONG* UsageLength,
      PHIDP_PREPARSED_DATA PreparsedData,
      PCHAR Report,
      ULONG ReportLength);
  typedef NTSTATUS (__stdcall *HidPGetUsageValueFunc)(
      HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
      USAGE Usage, PULONG UsageValue, PHIDP_PREPARSED_DATA PreparsedData,
      PCHAR Report, ULONG ReportLength);
  typedef NTSTATUS (__stdcall *HidPGetScaledUsageValueFunc)(
      HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
      USAGE Usage, PLONG UsageValue, PHIDP_PREPARSED_DATA PreparsedData,
      PCHAR Report, ULONG ReportLength);
  typedef BOOLEAN (__stdcall *HidDGetStringFunc)(
      HANDLE HidDeviceObject, PVOID Buffer, ULONG BufferLength);

  // Get functions from dynamically loaded hid.dll. Returns true if loading was
  // successful.
  bool GetHidDllFunctions();

  base::ScopedNativeLibrary hid_dll_;
  scoped_ptr<base::win::MessageWindow> window_;
  bool rawinput_available_;
  bool filter_xinput_;
  bool events_monitored_;

  std::map<HANDLE, RawGamepadInfo*> controllers_;

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

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
