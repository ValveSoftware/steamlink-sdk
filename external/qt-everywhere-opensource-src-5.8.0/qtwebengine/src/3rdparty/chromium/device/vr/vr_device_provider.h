// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_PROVIDER_H
#define DEVICE_VR_VR_DEVICE_PROVIDER_H

#include <vector>

namespace device {

class VRDevice;

class VRDeviceProvider {
 public:
  VRDeviceProvider() {}
  virtual ~VRDeviceProvider() {}

  virtual void GetDevices(std::vector<VRDevice*>* devices) = 0;

  // If the VR API requires initialization that should happen here.
  virtual void Initialize() = 0;
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_PROVIDER_H
