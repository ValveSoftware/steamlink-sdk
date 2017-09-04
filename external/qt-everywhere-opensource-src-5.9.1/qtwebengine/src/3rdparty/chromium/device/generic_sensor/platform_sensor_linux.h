// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_LINUX_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_LINUX_H_

#include "device/generic_sensor/platform_sensor.h"

namespace base {
class RepeatingTimer;
class SingleThreadTaskRunner;
class Thread;
}

namespace device {

class SensorReader;
struct SensorDataLinux;

class PlatformSensorLinux : public PlatformSensor {
 public:
  PlatformSensorLinux(
      mojom::SensorType type,
      mojo::ScopedSharedBufferMapping mapping,
      PlatformSensorProvider* provider,
      const SensorDataLinux& data,
      std::unique_ptr<SensorReader> sensor_reader,
      scoped_refptr<base::SingleThreadTaskRunner> polling_thread_task_runner);

  // Thread safe.
  mojom::ReportingMode GetReportingMode() override;

 protected:
  ~PlatformSensorLinux() override;
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;

 private:
  void BeginPoll(const PlatformSensorConfiguration& configuration);
  void StopPoll();

  // Triggers |sensor_reader_| to read new sensor data.
  // If new data is read, UpdateSensorReading() is called.
  void PollForReadingData();

  // Owned timer to be deleted on a polling thread.
  base::RepeatingTimer* timer_;

  const PlatformSensorConfiguration default_configuration_;
  const mojom::ReportingMode reporting_mode_;

  // A sensor reader that reads values from sensor files
  // and stores them to a SensorReading structure.
  std::unique_ptr<SensorReader> sensor_reader_;

  // A task runner that is used to poll sensor data.
  scoped_refptr<base::SingleThreadTaskRunner> polling_thread_task_runner_;

  // Stores previously read values that are used to
  // determine whether the recent values are changed
  // and IPC can be notified that updates are available.
  SensorReading old_values_;

  base::WeakPtrFactory<PlatformSensorLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorLinux);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_LINUX_H_
