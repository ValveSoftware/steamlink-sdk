// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_LINUX_H_
#define DEVICE_HID_HID_SERVICE_LINUX_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "device/hid/device_monitor_linux.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

struct udev_device;

namespace device {

class HidConnection;

class HidServiceLinux : public HidService,
                        public DeviceMonitorLinux::Observer {
 public:
  HidServiceLinux();

  virtual scoped_refptr<HidConnection> Connect(const HidDeviceId& device_id)
      OVERRIDE;

  // Implements DeviceMonitorLinux::Observer:
  virtual void OnDeviceAdded(udev_device* device) OVERRIDE;
  virtual void OnDeviceRemoved(udev_device* device) OVERRIDE;

 private:
  virtual ~HidServiceLinux();

  static bool FindHidrawDevNode(udev_device* parent, std::string* result);

  DISALLOW_COPY_AND_ASSIGN(HidServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_LINUX_H_
