// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PUBLIC_INTERFACES_SENSOR_STRUCT_TRAITS_H_
#define DEVICE_GENERIC_SENSOR_PUBLIC_INTERFACES_SENSOR_STRUCT_TRAITS_H_

#include "device/generic_sensor/public/cpp/platform_sensor_configuration.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom.h"

namespace mojo {

template <>
struct StructTraits<device::mojom::SensorConfigurationDataView,
                    device::PlatformSensorConfiguration> {
  static double frequency(const device::PlatformSensorConfiguration& input) {
    return input.frequency();
  }

  static bool Read(device::mojom::SensorConfigurationDataView data,
                   device::PlatformSensorConfiguration* out);
};

}  // namespace mojo

#endif  // DEVICE_GENERIC_SENSOR_PUBLIC_INTERFACES_SENSOR_STRUCT_TRAITS_H_
