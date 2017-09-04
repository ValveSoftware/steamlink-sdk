// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_provider_mac.h"

#include "base/memory/singleton.h"
#include "device/generic_sensor/platform_sensor_ambient_light_mac.h"

namespace device {

// static
PlatformSensorProviderMac* PlatformSensorProviderMac::GetInstance() {
  return base::Singleton<
      PlatformSensorProviderMac,
      base::LeakySingletonTraits<PlatformSensorProviderMac>>::get();
}

PlatformSensorProviderMac::PlatformSensorProviderMac() = default;

PlatformSensorProviderMac::~PlatformSensorProviderMac() = default;

void PlatformSensorProviderMac::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  // Create Sensors here.
  switch (type) {
    case mojom::SensorType::AMBIENT_LIGHT: {
      scoped_refptr<PlatformSensor> sensor =
          new PlatformSensorAmbientLightMac(type, std::move(mapping), this);
      callback.Run(std::move(sensor));
      break;
    }
    default:
      NOTIMPLEMENTED();
      callback.Run(nullptr);
  }
}

}  // namespace device
