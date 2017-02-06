// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_mac.h"

#include <stdint.h>
#include <string.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDKeys.h>

using blink::WebGamepad;
using blink::WebGamepads;

namespace device {

namespace {

void CopyNSStringAsUTF16LittleEndian(NSString* src,
                                     blink::WebUChar* dest,
                                     size_t dest_len) {
  NSData* as16 = [src dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
  memset(dest, 0, dest_len);
  [as16 getBytes:dest length:dest_len - sizeof(blink::WebUChar)];
}

NSDictionary* DeviceMatching(uint32_t usage_page, uint32_t usage) {
  return [NSDictionary
      dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedInt:usage_page],
                                   base::mac::CFToNSCast(
                                       CFSTR(kIOHIDDeviceUsagePageKey)),
                                   [NSNumber numberWithUnsignedInt:usage],
                                   base::mac::CFToNSCast(
                                       CFSTR(kIOHIDDeviceUsageKey)),
                                   nil];
}

float NormalizeAxis(CFIndex value, CFIndex min, CFIndex max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

float NormalizeUInt8Axis(uint8_t value, uint8_t min, uint8_t max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

float NormalizeUInt16Axis(uint16_t value, uint16_t min, uint16_t max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

float NormalizeUInt32Axis(uint32_t value, uint32_t min, uint32_t max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

// http://www.usb.org/developers/hidpage
const uint32_t kGenericDesktopUsagePage = 0x01;
const uint32_t kGameControlsUsagePage = 0x05;
const uint32_t kButtonUsagePage = 0x09;
const uint32_t kJoystickUsageNumber = 0x04;
const uint32_t kGameUsageNumber = 0x05;
const uint32_t kMultiAxisUsageNumber = 0x08;
const uint32_t kAxisMinimumUsageNumber = 0x30;

}  // namespace

GamepadPlatformDataFetcherMac::GamepadPlatformDataFetcherMac()
    : enabled_(true), paused_(false) {
  memset(associated_, 0, sizeof(associated_));

  xbox_fetcher_.reset(new XboxDataFetcher(this));
  if (!xbox_fetcher_->RegisterForNotifications())
    xbox_fetcher_.reset();

  hid_manager_ref_.reset(
      IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));
  if (CFGetTypeID(hid_manager_ref_) != IOHIDManagerGetTypeID()) {
    enabled_ = false;
    return;
  }

  base::scoped_nsobject<NSArray> criteria(
      [[NSArray alloc] initWithObjects:DeviceMatching(kGenericDesktopUsagePage,
                                                      kJoystickUsageNumber),
                                       DeviceMatching(kGenericDesktopUsagePage,
                                                      kGameUsageNumber),
                                       DeviceMatching(kGenericDesktopUsagePage,
                                                      kMultiAxisUsageNumber),
                                       nil]);
  IOHIDManagerSetDeviceMatchingMultiple(hid_manager_ref_,
                                        base::mac::NSToCFCast(criteria));

  RegisterForNotifications();
}

void GamepadPlatformDataFetcherMac::RegisterForNotifications() {
  // Register for plug/unplug notifications.
  IOHIDManagerRegisterDeviceMatchingCallback(hid_manager_ref_,
                                             DeviceAddCallback, this);
  IOHIDManagerRegisterDeviceRemovalCallback(hid_manager_ref_,
                                            DeviceRemoveCallback, this);

  // Register for value change notifications.
  IOHIDManagerRegisterInputValueCallback(hid_manager_ref_, ValueChangedCallback,
                                         this);

  IOHIDManagerScheduleWithRunLoop(hid_manager_ref_, CFRunLoopGetMain(),
                                  kCFRunLoopDefaultMode);

  enabled_ = IOHIDManagerOpen(hid_manager_ref_, kIOHIDOptionsTypeNone) ==
             kIOReturnSuccess;

  if (xbox_fetcher_)
    xbox_fetcher_->RegisterForNotifications();
}

void GamepadPlatformDataFetcherMac::UnregisterFromNotifications() {
  IOHIDManagerUnscheduleFromRunLoop(hid_manager_ref_, CFRunLoopGetCurrent(),
                                    kCFRunLoopDefaultMode);
  IOHIDManagerClose(hid_manager_ref_, kIOHIDOptionsTypeNone);
  if (xbox_fetcher_)
    xbox_fetcher_->UnregisterFromNotifications();
}

void GamepadPlatformDataFetcherMac::PauseHint(bool pause) {
  paused_ = pause;
}

GamepadPlatformDataFetcherMac::~GamepadPlatformDataFetcherMac() {
  UnregisterFromNotifications();
}

GamepadPlatformDataFetcherMac*
GamepadPlatformDataFetcherMac::InstanceFromContext(void* context) {
  return reinterpret_cast<GamepadPlatformDataFetcherMac*>(context);
}

void GamepadPlatformDataFetcherMac::DeviceAddCallback(void* context,
                                                      IOReturn result,
                                                      void* sender,
                                                      IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceAdd(ref);
}

void GamepadPlatformDataFetcherMac::DeviceRemoveCallback(void* context,
                                                         IOReturn result,
                                                         void* sender,
                                                         IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceRemove(ref);
}

void GamepadPlatformDataFetcherMac::ValueChangedCallback(void* context,
                                                         IOReturn result,
                                                         void* sender,
                                                         IOHIDValueRef ref) {
  InstanceFromContext(context)->ValueChanged(ref);
}

bool GamepadPlatformDataFetcherMac::CheckCollection(IOHIDElementRef element) {
  // Check that a parent collection of this element matches one of the usage
  // numbers that we are looking for.
  while ((element = IOHIDElementGetParent(element)) != NULL) {
    uint32_t usage_page = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);
    if (usage_page == kGenericDesktopUsagePage) {
      if (usage == kJoystickUsageNumber || usage == kGameUsageNumber ||
          usage == kMultiAxisUsageNumber) {
        return true;
      }
    }
  }
  return false;
}

bool GamepadPlatformDataFetcherMac::AddButtonsAndAxes(NSArray* elements,
                                                      size_t slot) {
  WebGamepad& pad = pad_state_[slot].data;
  AssociatedData& associated = associated_[slot];
  CHECK(!associated.is_xbox);

  pad.axesLength = 0;
  pad.buttonsLength = 0;
  pad.timestamp = 0;
  memset(pad.axes, 0, sizeof(pad.axes));
  memset(pad.buttons, 0, sizeof(pad.buttons));

  bool mapped_all_axes = true;

  for (id elem in elements) {
    IOHIDElementRef element = reinterpret_cast<IOHIDElementRef>(elem);
    if (!CheckCollection(element))
      continue;

    uint32_t usage_page = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);
    if (IOHIDElementGetType(element) == kIOHIDElementTypeInput_Button &&
        usage_page == kButtonUsagePage) {
      uint32_t button_index = usage - 1;
      if (button_index < WebGamepad::buttonsLengthCap) {
        associated.hid.button_elements[button_index] = element;
        pad.buttonsLength = std::max(pad.buttonsLength, button_index + 1);
      }
    } else if (IOHIDElementGetType(element) == kIOHIDElementTypeInput_Misc) {
      uint32_t axis_index = usage - kAxisMinimumUsageNumber;
      if (axis_index < WebGamepad::axesLengthCap) {
        associated.hid.axis_elements[axis_index] = element;
        pad.axesLength = std::max(pad.axesLength, axis_index + 1);
      } else {
        mapped_all_axes = false;
      }
    }
  }

  if (!mapped_all_axes) {
    // For axes who's usage puts them outside the standard axesLengthCap range.
    uint32_t next_index = 0;
    for (id elem in elements) {
      IOHIDElementRef element = reinterpret_cast<IOHIDElementRef>(elem);
      if (!CheckCollection(element))
        continue;

      uint32_t usage_page = IOHIDElementGetUsagePage(element);
      uint32_t usage = IOHIDElementGetUsage(element);
      if (IOHIDElementGetType(element) == kIOHIDElementTypeInput_Misc &&
          usage - kAxisMinimumUsageNumber >= WebGamepad::axesLengthCap &&
          usage_page <= kGameControlsUsagePage) {
        for (; next_index < WebGamepad::axesLengthCap; ++next_index) {
          if (associated.hid.axis_elements[next_index] == NULL)
            break;
        }
        if (next_index < WebGamepad::axesLengthCap) {
          associated.hid.axis_elements[next_index] = element;
          pad.axesLength = std::max(pad.axesLength, next_index + 1);
        }
      }

      if (next_index >= WebGamepad::axesLengthCap)
        break;
    }
  }

  for (uint32_t axis_index = 0; axis_index < pad.axesLength; ++axis_index) {
    IOHIDElementRef element = associated.hid.axis_elements[axis_index];
    if (element != NULL) {
      CFIndex axis_min = IOHIDElementGetLogicalMin(element);
      CFIndex axis_max = IOHIDElementGetLogicalMax(element);

      // Some HID axes report a logical range of -1 to 0 signed, which must be
      // interpreted as 0 to -1 unsigned for correct normalization behavior.
      if (axis_min == -1 && axis_max == 0) {
        axis_max = -1;
        axis_min = 0;
      }

      associated.hid.axis_minimums[axis_index] = axis_min;
      associated.hid.axis_maximums[axis_index] = axis_max;
      associated.hid.axis_report_sizes[axis_index] =
          IOHIDElementGetReportSize(element);
    }
  }

  return (pad.axesLength > 0 || pad.buttonsLength > 0);
}

size_t GamepadPlatformDataFetcherMac::GetEmptySlot() {
  // Find a free slot for this device.
  for (size_t slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (!pad_state_[slot].data.connected)
      return slot;
  }
  return WebGamepads::itemsLengthCap;
}

size_t GamepadPlatformDataFetcherMac::GetSlotForDevice(IOHIDDeviceRef device) {
  for (size_t slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    // If we already have this device, and it's already connected, don't do
    // anything now.
    if (pad_state_[slot].data.connected && !associated_[slot].is_xbox &&
        associated_[slot].hid.device_ref == device)
      return WebGamepads::itemsLengthCap;
  }
  return GetEmptySlot();
}

size_t GamepadPlatformDataFetcherMac::GetSlotForXboxDevice(
    XboxController* device) {
  for (size_t slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (associated_[slot].is_xbox &&
        associated_[slot].xbox.location_id == device->location_id()) {
      if (pad_state_[slot].data.connected) {
        // The device is already connected. No idea why we got a second "device
        // added" call, but let's not add it twice.
        DCHECK_EQ(associated_[slot].xbox.device, device);
        return WebGamepads::itemsLengthCap;
      } else {
        // A device with the same location ID was previously connected, so put
        // it in the same slot.
        return slot;
      }
    }
  }
  return GetEmptySlot();
}

void GamepadPlatformDataFetcherMac::DeviceAdd(IOHIDDeviceRef device) {
  using base::mac::CFToNSCast;
  using base::mac::CFCastStrict;

  if (!enabled_)
    return;

  // Find an index for this device.
  size_t slot = GetSlotForDevice(device);

  // We can't handle this many connected devices.
  if (slot == WebGamepads::itemsLengthCap)
    return;

  // Clear some state that may have been left behind by previous gamepads
  memset(&associated_[slot], 0, sizeof(AssociatedData));

  NSNumber* vendor_id = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey))));
  NSNumber* product_id = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey))));
  NSString* product = CFToNSCast(CFCastStrict<CFStringRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey))));
  int vendor_int = [vendor_id intValue];
  int product_int = [product_id intValue];

  char vendor_as_str[5], product_as_str[5];
  snprintf(vendor_as_str, sizeof(vendor_as_str), "%04x", vendor_int);
  snprintf(product_as_str, sizeof(product_as_str), "%04x", product_int);
  pad_state_[slot].mapper =
      GetGamepadStandardMappingFunction(vendor_as_str, product_as_str);

  NSString* ident = [NSString
      stringWithFormat:@"%@ (%sVendor: %04x Product: %04x)", product,
                       pad_state_[slot].mapper ? "STANDARD GAMEPAD " : "",
                       vendor_int, product_int];
  CopyNSStringAsUTF16LittleEndian(ident, pad_state_[slot].data.id,
                                  sizeof(pad_state_[slot].data.id));

  if (pad_state_[slot].mapper) {
    CopyNSStringAsUTF16LittleEndian(@"standard", pad_state_[slot].data.mapping,
                                    sizeof(pad_state_[slot].data.mapping));
  } else {
    pad_state_[slot].data.mapping[0] = 0;
  }

  base::ScopedCFTypeRef<CFArrayRef> elements(
      IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone));

  if (!AddButtonsAndAxes(CFToNSCast(elements), slot))
    return;

  associated_[slot].hid.device_ref = device;
  pad_state_[slot].data.connected = true;
  pad_state_[slot].axis_mask = 0;
  pad_state_[slot].button_mask = 0;
}

void GamepadPlatformDataFetcherMac::DeviceRemove(IOHIDDeviceRef device) {
  if (!enabled_)
    return;

  // Find the index for this device.
  size_t slot;
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (pad_state_[slot].data.connected && !associated_[slot].is_xbox &&
        associated_[slot].hid.device_ref == device)
      break;
  }
  DCHECK(slot < WebGamepads::itemsLengthCap);
  // Leave associated device_ref so that it will be reconnected in the same
  // location. Simply mark it as disconnected.
  pad_state_[slot].data.connected = false;
}

void GamepadPlatformDataFetcherMac::ValueChanged(IOHIDValueRef value) {
  if (!enabled_ || paused_)
    return;

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDDeviceRef device = IOHIDElementGetDevice(element);

  // Find device slot.
  size_t slot;
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (pad_state_[slot].data.connected && !associated_[slot].is_xbox &&
        associated_[slot].hid.device_ref == device)
      break;
  }
  if (slot == WebGamepads::itemsLengthCap)
    return;

  WebGamepad& pad = pad_state_[slot].data;
  AssociatedData& associated = associated_[slot];

  uint32_t value_length = IOHIDValueGetLength(value);
  if (value_length > 4) {
    // Workaround for bizarre issue with PS3 controllers that try to return
    // massive (30+ byte) values and crash IOHIDValueGetIntegerValue
    return;
  }

  // Find and fill in the associated button event, if any.
  for (size_t i = 0; i < pad.buttonsLength; ++i) {
    if (associated.hid.button_elements[i] == element) {
      pad.buttons[i].pressed = IOHIDValueGetIntegerValue(value);
      pad.buttons[i].value = pad.buttons[i].pressed ? 1.f : 0.f;
      pad.timestamp = std::max(pad.timestamp, IOHIDValueGetTimeStamp(value));
      return;
    }
  }

  // Find and fill in the associated axis event, if any.
  for (size_t i = 0; i < pad.axesLength; ++i) {
    if (associated.hid.axis_elements[i] == element) {
      CFIndex axis_min = associated.hid.axis_minimums[i];
      CFIndex axis_max = associated.hid.axis_maximums[i];
      CFIndex axis_value = IOHIDValueGetIntegerValue(value);

      if (axis_min > axis_max) {
        // We'll need to interpret this axis as unsigned during normalization.
        switch (associated.hid.axis_report_sizes[i]) {
          case 8:
            pad.axes[i] = NormalizeUInt8Axis(axis_value, axis_min, axis_max);
            break;
          case 16:
            pad.axes[i] = NormalizeUInt16Axis(axis_value, axis_min, axis_max);
            break;
          case 32:
            pad.axes[i] = NormalizeUInt32Axis(axis_value, axis_min, axis_max);
            break;
        }
      } else {
        pad.axes[i] = NormalizeAxis(axis_value, axis_min, axis_max);
      }

      pad.timestamp = std::max(pad.timestamp, IOHIDValueGetTimeStamp(value));
      return;
    }
  }
}

void GamepadPlatformDataFetcherMac::XboxDeviceAdd(XboxController* device) {
  if (!enabled_)
    return;

  size_t slot = GetSlotForXboxDevice(device);

  // We can't handle this many connected devices.
  if (slot == WebGamepads::itemsLengthCap)
    return;

  device->SetLEDPattern(
      (XboxController::LEDPattern)(XboxController::LED_FLASH_TOP_LEFT + slot));

  NSString* ident = [NSString
      stringWithFormat:@"%@ (STANDARD GAMEPAD Vendor: %04x Product: %04x)",
                       device->GetControllerType() ==
                               XboxController::XBOX_360_CONTROLLER
                           ? @"Xbox 360 Controller"
                           : @"Xbox One Controller",
                       device->GetProductId(), device->GetVendorId()];
  CopyNSStringAsUTF16LittleEndian(ident, pad_state_[slot].data.id,
                                  sizeof(pad_state_[slot].data.id));

  CopyNSStringAsUTF16LittleEndian(@"standard", pad_state_[slot].data.mapping,
                                  sizeof(pad_state_[slot].data.mapping));

  associated_[slot].is_xbox = true;
  associated_[slot].xbox.device = device;
  associated_[slot].xbox.location_id = device->location_id();
  pad_state_[slot].data.connected = true;
  pad_state_[slot].data.axesLength = 4;
  pad_state_[slot].data.buttonsLength = 17;
  pad_state_[slot].data.timestamp = 0;
  pad_state_[slot].mapper = 0;
  pad_state_[slot].axis_mask = 0;
  pad_state_[slot].button_mask = 0;
}

void GamepadPlatformDataFetcherMac::XboxDeviceRemove(XboxController* device) {
  if (!enabled_)
    return;

  // Find the index for this device.
  size_t slot;
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (pad_state_[slot].data.connected && associated_[slot].is_xbox &&
        associated_[slot].xbox.device == device)
      break;
  }
  DCHECK(slot < WebGamepads::itemsLengthCap);
  // Leave associated location id so that the controller will be reconnected in
  // the same slot if it is plugged in again. Simply mark it as disconnected.
  pad_state_[slot].data.connected = false;
}

void GamepadPlatformDataFetcherMac::XboxValueChanged(
    XboxController* device,
    const XboxController::Data& data) {
  // Find device slot.
  size_t slot;
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (pad_state_[slot].data.connected && associated_[slot].is_xbox &&
        associated_[slot].xbox.device == device)
      break;
  }
  if (slot == WebGamepads::itemsLengthCap)
    return;

  WebGamepad& pad = pad_state_[slot].data;

  for (size_t i = 0; i < 6; i++) {
    pad.buttons[i].pressed = data.buttons[i];
    pad.buttons[i].value = data.buttons[i] ? 1.0f : 0.0f;
  }
  pad.buttons[6].pressed = data.triggers[0] > kDefaultButtonPressedThreshold;
  pad.buttons[6].value = data.triggers[0];
  pad.buttons[7].pressed = data.triggers[1] > kDefaultButtonPressedThreshold;
  pad.buttons[7].value = data.triggers[1];
  for (size_t i = 8; i < 17; i++) {
    pad.buttons[i].pressed = data.buttons[i - 2];
    pad.buttons[i].value = data.buttons[i - 2] ? 1.0f : 0.0f;
  }
  for (size_t i = 0; i < arraysize(data.axes); i++) {
    pad.axes[i] = data.axes[i];
  }

  pad.timestamp = base::TimeTicks::Now().ToInternalValue();
}

void GamepadPlatformDataFetcherMac::GetGamepadData(WebGamepads* pads, bool) {
  if (!enabled_ && !xbox_fetcher_) {
    pads->length = 0;
    return;
  }

  pads->length = WebGamepads::itemsLengthCap;
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i)
    MapAndSanitizeGamepadData(&pad_state_[i], &pads->items[i]);
}

}  // namespace device
