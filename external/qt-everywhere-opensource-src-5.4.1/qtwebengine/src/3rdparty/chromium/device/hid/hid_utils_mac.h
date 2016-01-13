// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_UTILS_MAC_H_
#define DEVICE_HID_HID_UTILS_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <stdint.h>

#include <string>

namespace device {

int32_t GetHidIntProperty(IOHIDDeviceRef device, CFStringRef key);

std::string GetHidStringProperty(IOHIDDeviceRef device, CFStringRef key);

bool TryGetHidIntProperty(IOHIDDeviceRef device,
                          CFStringRef key,
                          int32_t* result);

bool TryGetHidStringProperty(IOHIDDeviceRef device,
                             CFStringRef key,
                             std::string* result);

}  // namespace device

#endif  // DEVICE_HID_HID_UTILS_MAC_H_
