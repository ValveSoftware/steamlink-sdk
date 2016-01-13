// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/device/device_manager.h"

#if defined(USE_UDEV)
#include "ui/events/ozone/device/udev/device_manager_udev.h"
#else
#include "ui/events/ozone/device/device_manager_manual.h"
#endif

namespace ui {

scoped_ptr<DeviceManager> CreateDeviceManager() {
#if defined(USE_UDEV)
  return scoped_ptr<DeviceManager>(new DeviceManagerUdev());
#else
  return scoped_ptr<DeviceManager>(new DeviceManagerManual());
#endif
}

}  // namespace ui
