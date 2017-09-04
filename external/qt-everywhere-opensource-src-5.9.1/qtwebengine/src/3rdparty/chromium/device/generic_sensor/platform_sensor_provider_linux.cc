// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_provider_linux.h"

#include "base/memory/singleton.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "device/generic_sensor/linux/platform_sensor_utils_linux.h"
#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/generic_sensor/platform_sensor_linux.h"

namespace device {

// static
PlatformSensorProviderLinux* PlatformSensorProviderLinux::GetInstance() {
  return base::Singleton<
      PlatformSensorProviderLinux,
      base::LeakySingletonTraits<PlatformSensorProviderLinux>>::get();
}

PlatformSensorProviderLinux::PlatformSensorProviderLinux() = default;

PlatformSensorProviderLinux::~PlatformSensorProviderLinux() = default;

void PlatformSensorProviderLinux::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  SensorDataLinux data;
  if (!InitSensorData(type, &data)) {
    callback.Run(nullptr);
    return;
  }

  if (!polling_thread_)
    polling_thread_.reset(new base::Thread("Sensor polling thread"));

  if (!polling_thread_->IsRunning()) {
    if (!polling_thread_->StartWithOptions(
            base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
      callback.Run(nullptr);
      return;
    }
    polling_thread_task_runner_ = polling_thread_->task_runner();
  }

  base::PostTaskAndReplyWithResult(
      polling_thread_task_runner_.get(), FROM_HERE,
      base::Bind(SensorReader::Create, data),
      base::Bind(&PlatformSensorProviderLinux::SensorReaderFound,
                 base::Unretained(this), type, base::Passed(&mapping), callback,
                 data));
}

void PlatformSensorProviderLinux::SensorReaderFound(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const PlatformSensorProviderBase::CreateSensorCallback& callback,
    const SensorDataLinux& data,
    std::unique_ptr<SensorReader> sensor_reader) {
  DCHECK(CalledOnValidThread());

  if (!sensor_reader) {
    // If there are no sensors, stop polling thread.
    if (!HasSensors())
      AllSensorsRemoved();
    callback.Run(nullptr);
    return;
  }

  callback.Run(new PlatformSensorLinux(type, std::move(mapping), this, data,
                                       std::move(sensor_reader),
                                       polling_thread_task_runner_));
}

void PlatformSensorProviderLinux::SetFileTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  DCHECK(CalledOnValidThread());
  if (!file_task_runner_)
    file_task_runner_ = file_task_runner;
}

void PlatformSensorProviderLinux::AllSensorsRemoved() {
  DCHECK(CalledOnValidThread());
  DCHECK(file_task_runner_);
  // When there are no sensors left, the polling thread must be stopped.
  // Stop() can only be called on a different thread that allows io.
  // Thus, browser's file thread is used for this purpose.
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PlatformSensorProviderLinux::StopPollingThread,
                            base::Unretained(this)));
}

void PlatformSensorProviderLinux::StopPollingThread() {
  DCHECK(file_task_runner_);
  DCHECK(file_task_runner_->BelongsToCurrentThread());
  polling_thread_->Stop();
}

}  // namespace device
