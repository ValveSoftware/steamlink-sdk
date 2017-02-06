// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"

namespace device {

unsigned int VRDevice::next_id_ = 1;

VRDevice::VRDevice(VRDeviceProvider* provider)
    : provider_(provider), id_(next_id_) {
  // Prevent wraparound. Devices with this ID will be treated as invalid.
  if (next_id_ != VR_DEVICE_LAST_ID)
    next_id_++;
}

VRDevice::~VRDevice() {}

}  // namespace device
