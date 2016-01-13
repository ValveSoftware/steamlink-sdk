// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libudev.h>

#include "device/udev_linux/udev.h"

device::LibUdevWrapper libudev;

namespace device {

void UdevDeleter::operator()(udev* dev) const {
  libudev.udev_unref(dev);
}

void UdevEnumerateDeleter::operator()(udev_enumerate* enumerate) const {
  libudev.udev_enumerate_unref(enumerate);
}

void UdevDeviceDeleter::operator()(udev_device* device) const {
  libudev.udev_device_unref(device);
}

void UdevMonitorDeleter::operator()(udev_monitor* monitor) const {
  libudev.udev_monitor_unref(monitor);
}

}  // namespace device
