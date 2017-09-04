// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/public/cpp/sensor_struct_traits.h"

namespace mojo {

// static
bool StructTraits<device::mojom::SensorConfigurationDataView,
                  device::PlatformSensorConfiguration>::
    Read(device::mojom::SensorConfigurationDataView data,
         device::PlatformSensorConfiguration* out) {
  // Maximum allowed frequency is capped to 60Hz.
  if (data.frequency() >
          device::mojom::SensorConfiguration::kMaxAllowedFrequency ||
      data.frequency() <= 0.0) {
    return false;
  }

  out->set_frequency(data.frequency());
  return true;
}

}  // namespace mojo
