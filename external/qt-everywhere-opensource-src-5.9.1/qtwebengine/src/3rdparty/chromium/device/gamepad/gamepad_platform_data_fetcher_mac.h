// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <stddef.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"

#if defined(__OBJC__)
@class NSArray;
#else
class NSArray;
#endif

namespace device {

class GamepadPlatformDataFetcherMac : public GamepadDataFetcher {
 public:
  typedef GamepadDataFetcherFactoryImpl<GamepadPlatformDataFetcherMac,
                                        GAMEPAD_SOURCE_MAC_HID>
      Factory;

  GamepadPlatformDataFetcherMac();
  ~GamepadPlatformDataFetcherMac() override;

  GamepadSource source() override;

  void GetGamepadData(bool devices_changed_hint) override;
  void PauseHint(bool paused) override;

 private:
  bool enabled_;
  bool paused_;
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

  void OnAddedToProvider() override;

  size_t GetEmptySlot();
  size_t GetSlotForDevice(IOHIDDeviceRef device);

  void DeviceAdd(IOHIDDeviceRef device);
  bool CheckCollection(IOHIDElementRef element);
  bool AddButtonsAndAxes(NSArray* elements, PadState* state, size_t slot);
  void DeviceRemove(IOHIDDeviceRef device);
  void ValueChanged(IOHIDValueRef value);

  void RegisterForNotifications();
  void UnregisterFromNotifications();

  // Side-band data that's not passed to the consumer, but we need to maintain
  // to update data_.
  struct AssociatedData {
    int location_id;
    IOHIDDeviceRef device_ref;
    IOHIDElementRef button_elements[blink::WebGamepad::buttonsLengthCap];
    IOHIDElementRef axis_elements[blink::WebGamepad::axesLengthCap];
    CFIndex axis_minimums[blink::WebGamepad::axesLengthCap];
    CFIndex axis_maximums[blink::WebGamepad::axesLengthCap];
    CFIndex axis_report_sizes[blink::WebGamepad::axesLengthCap];
  };
  AssociatedData associated_[blink::WebGamepads::itemsLengthCap];

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherMac);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_
