// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_WIN_H_
#define DEVICE_HID_HID_SERVICE_WIN_H_

#include <map>

#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

namespace device {

class HidConnection;

class HidServiceWin : public HidService {
 public:
  HidServiceWin();

  virtual void GetDevices(std::vector<HidDeviceInfo>* devices) OVERRIDE;

  virtual scoped_refptr<HidConnection> Connect(const HidDeviceId& device_id)
      OVERRIDE;

 private:
  virtual ~HidServiceWin();

  void Enumerate();
  void PlatformAddDevice(const std::string& device_path);
  void PlatformRemoveDevice(const std::string& device_path);

  DISALLOW_COPY_AND_ASSIGN(HidServiceWin);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_WIN_H_
