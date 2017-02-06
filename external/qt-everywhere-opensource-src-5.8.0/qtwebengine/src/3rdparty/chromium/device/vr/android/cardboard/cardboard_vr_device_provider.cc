// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_vr_device_provider.h"

#include "device/vr/android/cardboard/cardboard_vr_device.h"

namespace device {

CardboardVRDeviceProvider::CardboardVRDeviceProvider() : VRDeviceProvider() {}

CardboardVRDeviceProvider::~CardboardVRDeviceProvider() {}

void CardboardVRDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  if (!cardboard_device_) {
    cardboard_device_.reset(new CardboardVRDevice(this));
  }

  devices->push_back(cardboard_device_.get());
}

void CardboardVRDeviceProvider::Initialize() {
  // No initialization needed for Cardboard devices.
}

}  // namespace device
