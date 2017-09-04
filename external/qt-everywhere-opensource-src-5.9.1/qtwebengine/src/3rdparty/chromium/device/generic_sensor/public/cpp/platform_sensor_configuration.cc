// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/public/cpp/platform_sensor_configuration.h"

#include "device/generic_sensor/public/interfaces/sensor.mojom.h"

namespace device {

PlatformSensorConfiguration::PlatformSensorConfiguration(double frequency)
    : frequency_(frequency) {
  DCHECK(frequency_ <= mojom::SensorConfiguration::kMaxAllowedFrequency &&
         frequency_ > 0.0);
}

PlatformSensorConfiguration::PlatformSensorConfiguration() = default;
PlatformSensorConfiguration::~PlatformSensorConfiguration() = default;

void PlatformSensorConfiguration::set_frequency(double frequency) {
  DCHECK(frequency_ <= mojom::SensorConfiguration::kMaxAllowedFrequency &&
         frequency_ > 0.0);
  frequency_ = frequency;
}

bool PlatformSensorConfiguration::operator==(
    const PlatformSensorConfiguration& other) const {
  return frequency_ == other.frequency();
}

bool PlatformSensorConfiguration::operator>(
    const PlatformSensorConfiguration& other) const {
  return frequency() > other.frequency();
}

}  // namespace device
