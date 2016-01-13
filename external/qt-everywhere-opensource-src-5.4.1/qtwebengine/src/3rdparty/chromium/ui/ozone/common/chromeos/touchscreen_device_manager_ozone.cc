// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/chromeos/touchscreen_device_manager_ozone.h"

#include "base/logging.h"

namespace ui {

TouchscreenDeviceManagerOzone::TouchscreenDeviceManagerOzone() {}

TouchscreenDeviceManagerOzone::~TouchscreenDeviceManagerOzone() {}

std::vector<TouchscreenDevice> TouchscreenDeviceManagerOzone::GetDevices() {
  NOTIMPLEMENTED();
  return std::vector<TouchscreenDevice>();
}

}  // namespace ui
