// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_FAKE_INPUT_SERVICE_LINUX_H_
#define DEVICE_HID_FAKE_INPUT_SERVICE_LINUX_H_

#include <string>

#include "base/macros.h"
#include "device/hid/input_service_linux.h"

namespace device {

class FakeInputServiceLinux : public InputServiceLinux {
 public:
  FakeInputServiceLinux();
  ~FakeInputServiceLinux() override;

  void AddDeviceForTesting(const InputDeviceInfo& info);
  void RemoveDeviceForTesting(const std::string& id);
  void ClearDeviceList();

 private:
  // InputServiceLinux override:
  void GetDevices(std::vector<InputDeviceInfo>* devices) override;

  DISALLOW_COPY_AND_ASSIGN(FakeInputServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_FAKE_INPUT_SERVICE_LINUX_H_
