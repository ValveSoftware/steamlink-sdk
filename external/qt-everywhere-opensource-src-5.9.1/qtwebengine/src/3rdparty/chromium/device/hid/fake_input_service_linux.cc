// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/fake_input_service_linux.h"

#include <string>
#include <vector>

namespace device {

FakeInputServiceLinux::FakeInputServiceLinux() {
}

FakeInputServiceLinux::~FakeInputServiceLinux() {
}

void FakeInputServiceLinux::AddDeviceForTesting(const InputDeviceInfo& info) {
  AddDevice(info);
}

void FakeInputServiceLinux::RemoveDeviceForTesting(const std::string& id) {
  RemoveDevice(id);
}

void FakeInputServiceLinux::ClearDeviceList() {
  devices_.clear();
}

void FakeInputServiceLinux::GetDevices(std::vector<InputDeviceInfo>* devices) {
  for (const auto& device : devices_)
    devices->push_back(device.second);
}

}  // namespace device