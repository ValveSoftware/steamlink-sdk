// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_utils_mac.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace device {

int32_t GetHidIntProperty(IOHIDDeviceRef device, CFStringRef key) {
  int32_t value;
  if (TryGetHidIntProperty(device, key, &value))
    return value;
  return 0;
}

std::string GetHidStringProperty(IOHIDDeviceRef device, CFStringRef key) {
  std::string value;
  TryGetHidStringProperty(device, key, &value);
  return value;
}

bool TryGetHidIntProperty(IOHIDDeviceRef device,
                          CFStringRef key,
                          int32_t* result) {
  CFNumberRef ref = base::mac::CFCast<CFNumberRef>(
      IOHIDDeviceGetProperty(device, key));
  return ref && CFNumberGetValue(ref, kCFNumberSInt32Type, result);
}

bool TryGetHidStringProperty(IOHIDDeviceRef device,
                             CFStringRef key,
                             std::string* result) {
  CFStringRef ref = base::mac::CFCast<CFStringRef>(
      IOHIDDeviceGetProperty(device, key));
  if (!ref) {
    return false;
  }
  *result = base::SysCFStringRefToUTF8(ref);
  return true;
}

}  // namespace device
