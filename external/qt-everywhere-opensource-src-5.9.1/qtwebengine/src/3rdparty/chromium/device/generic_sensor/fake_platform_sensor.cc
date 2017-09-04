// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/fake_platform_sensor.h"

namespace device {

FakePlatformSensor::FakePlatformSensor(mojom::SensorType type,
                                       mojo::ScopedSharedBufferMapping mapping,
                                       PlatformSensorProvider* provider)
    : PlatformSensor(type, std::move(mapping), provider), started_(false) {}

FakePlatformSensor::~FakePlatformSensor() = default;

mojom::ReportingMode FakePlatformSensor::GetReportingMode() {
  return mojom::ReportingMode::CONTINUOUS;
}

PlatformSensorConfiguration FakePlatformSensor::GetDefaultConfiguration() {
  return PlatformSensorConfiguration();
}

bool FakePlatformSensor::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  config_ = configuration;
  started_ = true;
  return started_;
}

void FakePlatformSensor::StopSensor() {
  started_ = false;
}

bool FakePlatformSensor::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() <= kMaxFrequencyValueForTests;
}

}  // namespace device
