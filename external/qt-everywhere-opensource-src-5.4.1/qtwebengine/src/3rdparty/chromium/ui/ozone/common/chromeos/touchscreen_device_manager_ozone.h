// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_TOUCHSCREEN_DEVICE_MANAGER_OZONE_H_
#define UI_OZONE_COMMON_TOUCHSCREEN_DEVICE_MANAGER_OZONE_H_

#include "base/macros.h"
#include "ui/display/types/chromeos/touchscreen_device_manager.h"

namespace ui {

class TouchscreenDeviceManagerOzone : public TouchscreenDeviceManager {
 public:
  TouchscreenDeviceManagerOzone();
  virtual ~TouchscreenDeviceManagerOzone();

  // TouchscreenDeviceManager:
  virtual std::vector<TouchscreenDevice> GetDevices() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchscreenDeviceManagerOzone);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_TOUCHSCREEN_DEVICE_MANAGER_OZONE_H_
