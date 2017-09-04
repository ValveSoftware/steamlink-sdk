// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DATA_LINUX_H_
#define DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DATA_LINUX_H_

#include "device/generic_sensor/generic_sensor_export.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom.h"

namespace device {

struct SensorReading;

// This structure represents a context that is used to
// create a type specific SensorReader and a concrete
// sensor that uses the SensorReader to read sensor
// data from files specified in the |sensor_file_names|.
struct DEVICE_GENERIC_SENSOR_EXPORT SensorDataLinux {
  using ReaderFunctor =
      base::Callback<void(double scaling, SensorReading& reading)>;

  SensorDataLinux();
  ~SensorDataLinux();
  SensorDataLinux(const SensorDataLinux& other);
  // Provides a base path to all sensors.
  const base::FilePath::CharType* base_path_sensor_linux;
  // Provides an array of sensor file names to be searched for.
  // Different sensors might have up to 3 different file name arrays.
  // One file must be found from each array.
  std::vector<std::vector<std::string>> sensor_file_names;
  // Scaling file to be found.
  std::string sensor_scale_name;
  // Used to apply scalings to raw sensor data.
  ReaderFunctor apply_scaling_func;
  // Reporting mode of a sensor.
  mojom::ReportingMode reporting_mode;
  // Default configuration of a sensor.
  PlatformSensorConfiguration default_configuration;
};

// Initializes a sensor type specific data.
bool InitSensorData(mojom::SensorType type, SensorDataLinux* data);

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DATA_LINUX_H_
