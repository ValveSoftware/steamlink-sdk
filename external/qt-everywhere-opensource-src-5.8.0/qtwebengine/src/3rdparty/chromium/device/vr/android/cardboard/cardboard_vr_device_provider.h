// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_CARDBOARD_VR_DEVICE_PROVIDER_H
#define DEVICE_VR_CARDBOARD_VR_DEVICE_PROVIDER_H

#include <map>
#include <memory>

#include "base/macros.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"

namespace device {

class CardboardVRDeviceProvider : public VRDeviceProvider {
 public:
  CardboardVRDeviceProvider();
  ~CardboardVRDeviceProvider() override;

  void GetDevices(std::vector<VRDevice*>* devices) override;
  void Initialize() override;

 private:
  std::unique_ptr<VRDevice> cardboard_device_;

  DISALLOW_COPY_AND_ASSIGN(CardboardVRDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_CARDBOARD_VR_DEVICE_PROVIDER_H
