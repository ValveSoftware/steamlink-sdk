// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_linux.h"

#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "device/generic_sensor/linux/platform_sensor_utils_linux.h"
#include "device/generic_sensor/linux/sensor_data_linux.h"

namespace device {

namespace {

// Checks if at least one value has been changed.
bool HaveValuesChanged(const SensorReading& lhs, const SensorReading& rhs) {
  return lhs.values[0] != rhs.values[0] || lhs.values[1] != rhs.values[1] ||
         lhs.values[2] != rhs.values[2];
}

}  // namespace

PlatformSensorLinux::PlatformSensorLinux(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    PlatformSensorProvider* provider,
    const SensorDataLinux& data,
    std::unique_ptr<SensorReader> sensor_reader,
    scoped_refptr<base::SingleThreadTaskRunner> polling_thread_task_runner_)
    : PlatformSensor(type, std::move(mapping), provider),
      timer_(new base::RepeatingTimer()),
      default_configuration_(data.default_configuration),
      reporting_mode_(data.reporting_mode),
      sensor_reader_(std::move(sensor_reader)),
      polling_thread_task_runner_(polling_thread_task_runner_),
      weak_factory_(this) {}

PlatformSensorLinux::~PlatformSensorLinux() {
  polling_thread_task_runner_->DeleteSoon(FROM_HERE, timer_);
}

mojom::ReportingMode PlatformSensorLinux::GetReportingMode() {
  return reporting_mode_;
}

bool PlatformSensorLinux::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return polling_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PlatformSensorLinux::BeginPoll,
                            weak_factory_.GetWeakPtr(), configuration));
}

void PlatformSensorLinux::StopSensor() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  polling_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PlatformSensorLinux::StopPoll, this));
}

bool PlatformSensorLinux::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // TODO(maksims): make this sensor dependent.
  // For example, in case of accelerometer, check current polling frequency
  // exposed by iio driver.
  return configuration.frequency() > 0 &&
         configuration.frequency() <=
             mojom::SensorConfiguration::kMaxAllowedFrequency;
}

PlatformSensorConfiguration PlatformSensorLinux::GetDefaultConfiguration() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return default_configuration_;
}

void PlatformSensorLinux::BeginPoll(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(polling_thread_task_runner_->BelongsToCurrentThread());
  timer_->Start(FROM_HERE, base::TimeDelta::FromMicroseconds(
                               base::Time::kMicrosecondsPerSecond /
                               configuration.frequency()),
                this, &PlatformSensorLinux::PollForReadingData);
}

void PlatformSensorLinux::StopPoll() {
  DCHECK(polling_thread_task_runner_->BelongsToCurrentThread());
  timer_->Stop();
}

void PlatformSensorLinux::PollForReadingData() {
  DCHECK(polling_thread_task_runner_->BelongsToCurrentThread());

  SensorReading reading;
  if (!sensor_reader_->ReadSensorReading(&reading)) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&PlatformSensorLinux::NotifySensorError, this));
    StopPoll();
    return;
  }

  bool notifyNeeded = false;
  if (GetReportingMode() == mojom::ReportingMode::ON_CHANGE) {
    if (!HaveValuesChanged(reading, old_values_))
      return;
    notifyNeeded = true;
  }

  old_values_ = reading;
  reading.timestamp = (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  UpdateSensorReading(reading, notifyNeeded);
}

}  // namespace device
