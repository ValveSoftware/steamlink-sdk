// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/mock_hid_service.h"

namespace device {

MockHidService::MockHidService() {}

MockHidService::~MockHidService() {
  // Shutdown() must be called before the base class destructor.
  Shutdown();
}

void MockHidService::AddDevice(scoped_refptr<HidDeviceInfo> info) {
  HidService::AddDevice(info);
}

void MockHidService::RemoveDevice(const HidDeviceId& device_id) {
  HidService::RemoveDevice(device_id);
}

void MockHidService::FirstEnumerationComplete() {
  HidService::FirstEnumerationComplete();
}

const std::map<HidDeviceId, scoped_refptr<HidDeviceInfo>>&
MockHidService::devices() const {
  return HidService::devices();
}

}  // namespace device
