// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_

#include "base/logging.h"
#include "device/generic_sensor/generic_sensor_export.h"

namespace device {

class DEVICE_GENERIC_SENSOR_EXPORT PlatformSensorConfiguration {
 public:
  PlatformSensorConfiguration();
  explicit PlatformSensorConfiguration(double frequency);
  ~PlatformSensorConfiguration();

  bool operator==(const PlatformSensorConfiguration& other) const;

  // Platform dependent implementations can override this operator if different
  // optimal configuration comparison is required. By default, only frequency is
  // used to compare two configurations.
  virtual bool operator>(const PlatformSensorConfiguration& other) const;

  void set_frequency(double frequency);
  double frequency() const { return frequency_; }

 private:
  double frequency_ = 1.0;  // 1 Hz by default.
};

}  // namespace device

#endif  // DEVICE_SENSORS_PLATFORM_SENSOR_CONFIGURATION_H_
