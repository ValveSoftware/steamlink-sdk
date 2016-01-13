// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_CHROMEOS_TOUCHSCREEN_DEVICE_MANAGER_H_
#define UI_DISPLAY_TYPES_CHROMEOS_TOUCHSCREEN_DEVICE_MANAGER_H_

#include <vector>

#include "ui/display/types/chromeos/touchscreen_device.h"

namespace ui {

// Implementations are responsible for querying and returning a list of avalable
// touchscreen devices.
class DISPLAY_TYPES_EXPORT TouchscreenDeviceManager {
 public:
  virtual ~TouchscreenDeviceManager() {}

  // Returns a list of available touchscreen devices. This call will query the
  // underlying system for an updated list of devices.
  virtual std::vector<TouchscreenDevice> GetDevices() = 0;
};

}  // namespace ui

#endif  // UI_DISPLAY_TYPES_CHROMEOS_TOUCHSCREEN_DEVICE_MANAGER_H_
