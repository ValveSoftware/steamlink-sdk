// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_H_
#define DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_H_

#include "device/generic_sensor/platform_sensor.h"

namespace device {

class FakePlatformSensor : public PlatformSensor {
 public:
  FakePlatformSensor(mojom::SensorType type,
                     mojo::ScopedSharedBufferMapping mapping,
                     PlatformSensorProvider* provider);

  mojom::ReportingMode GetReportingMode() override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;

  bool started() const { return started_; }

  using PlatformSensor::NotifySensorReadingChanged;
  using PlatformSensor::NotifySensorError;
  using PlatformSensor::config_map;

 protected:
  ~FakePlatformSensor() override;
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;

 private:
  static constexpr double kMaxFrequencyValueForTests = 50.0;

  PlatformSensorConfiguration config_;

  bool started_;

  DISALLOW_COPY_AND_ASSIGN(FakePlatformSensor);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_H
