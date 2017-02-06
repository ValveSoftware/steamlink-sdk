// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_H
#define DEVICE_VR_VR_DEVICE_H

#include "base/macros.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"

namespace blink {
struct WebHMDSensorState;
}

namespace ui {
class BaseWindow;
}

namespace device {

class VRDeviceProvider;

const unsigned int VR_DEVICE_LAST_ID = 0xFFFFFFFF;

class VRDevice {
 public:
  DEVICE_VR_EXPORT explicit VRDevice(VRDeviceProvider* provider);
  DEVICE_VR_EXPORT virtual ~VRDevice();

  DEVICE_VR_EXPORT VRDeviceProvider* provider() const { return provider_; }
  DEVICE_VR_EXPORT unsigned int id() const { return id_; }

  virtual VRDisplayPtr GetVRDevice() = 0;
  virtual VRPosePtr GetPose() = 0;
  virtual void ResetPose() = 0;

 private:
  VRDeviceProvider* provider_;
  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H
