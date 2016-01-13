// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "content/browser/gamepad/xbox_data_fetcher_mac.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#if defined(__OBJC__)
@class NSArray;
#else
class NSArray;
#endif

namespace content {

class GamepadPlatformDataFetcherMac : public GamepadDataFetcher,
                                      public XboxDataFetcher::Delegate {
 public:
  GamepadPlatformDataFetcherMac();
  virtual ~GamepadPlatformDataFetcherMac();
  virtual void GetGamepadData(blink::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;
  virtual void PauseHint(bool paused) OVERRIDE;

 private:
  bool enabled_;
  base::ScopedCFTypeRef<IOHIDManagerRef> hid_manager_ref_;

  static GamepadPlatformDataFetcherMac* InstanceFromContext(void* context);
  static void DeviceAddCallback(void* context,
                                IOReturn result,
                                void* sender,
                                IOHIDDeviceRef ref);
  static void DeviceRemoveCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDDeviceRef ref);
  static void ValueChangedCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDValueRef ref);

  size_t GetEmptySlot();
  size_t GetSlotForDevice(IOHIDDeviceRef device);
  size_t GetSlotForXboxDevice(XboxController* device);

  void DeviceAdd(IOHIDDeviceRef device);
  void AddButtonsAndAxes(NSArray* elements, size_t slot);
  void DeviceRemove(IOHIDDeviceRef device);
  void ValueChanged(IOHIDValueRef value);

  virtual void XboxDeviceAdd(XboxController* device) OVERRIDE;
  virtual void XboxDeviceRemove(XboxController* device) OVERRIDE;
  virtual void XboxValueChanged(XboxController* device,
                                const XboxController::Data& data) OVERRIDE;

  void RegisterForNotifications();
  void UnregisterFromNotifications();

  scoped_ptr<XboxDataFetcher> xbox_fetcher_;

  blink::WebGamepads data_;

  // Side-band data that's not passed to the consumer, but we need to maintain
  // to update data_.
  struct AssociatedData {
    bool is_xbox;
    union {
      struct {
        IOHIDDeviceRef device_ref;
        IOHIDElementRef button_elements[blink::WebGamepad::buttonsLengthCap];
        IOHIDElementRef axis_elements[blink::WebGamepad::axesLengthCap];
        CFIndex axis_minimums[blink::WebGamepad::axesLengthCap];
        CFIndex axis_maximums[blink::WebGamepad::axesLengthCap];
        // Function to map from device data to standard layout, if available.
        // May be null if no mapping is available.
        GamepadStandardMappingFunction mapper;
      } hid;
      struct {
        XboxController* device;
        UInt32 location_id;
      } xbox;
    };
  };
  AssociatedData associated_[blink::WebGamepads::itemsLengthCap];

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_
