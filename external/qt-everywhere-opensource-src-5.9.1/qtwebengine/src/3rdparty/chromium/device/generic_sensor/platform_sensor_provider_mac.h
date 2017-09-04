// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_

#include "device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorProviderMac : public PlatformSensorProvider {
 public:
  PlatformSensorProviderMac();
  ~PlatformSensorProviderMac() override;

  static PlatformSensorProviderMac* GetInstance();

 protected:
  void CreateSensorInternal(mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderMac);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_MAC_H_
