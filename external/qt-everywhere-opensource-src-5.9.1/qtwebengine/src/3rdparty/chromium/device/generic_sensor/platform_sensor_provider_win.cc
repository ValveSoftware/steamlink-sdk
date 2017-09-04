// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_provider_win.h"

#include "base/memory/singleton.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "device/generic_sensor/platform_sensor_win.h"

namespace device {

// static
PlatformSensorProviderWin* PlatformSensorProviderWin::GetInstance() {
  return base::Singleton<
      PlatformSensorProviderWin,
      base::LeakySingletonTraits<PlatformSensorProviderWin>>::get();
}

void PlatformSensorProviderWin::SetSensorManagerForTesting(
    base::win::ScopedComPtr<ISensorManager> sensor_manager) {
  sensor_manager_ = sensor_manager;
}

PlatformSensorProviderWin::PlatformSensorProviderWin() = default;
PlatformSensorProviderWin::~PlatformSensorProviderWin() = default;

void PlatformSensorProviderWin::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!StartSensorThread()) {
    callback.Run(nullptr);
    return;
  }

  base::PostTaskAndReplyWithResult(
      sensor_thread_->task_runner().get(), FROM_HERE,
      base::Bind(&PlatformSensorProviderWin::CreateSensorReader,
                 base::Unretained(this), type),
      base::Bind(&PlatformSensorProviderWin::SensorReaderCreated,
                 base::Unretained(this), type, base::Passed(&mapping),
                 callback));
}

bool PlatformSensorProviderWin::InitializeSensorManager() {
  if (sensor_manager_)
    return true;

  HRESULT hr = sensor_manager_.CreateInstance(CLSID_SensorManager);
  return SUCCEEDED(hr);
}

void PlatformSensorProviderWin::AllSensorsRemoved() {
  StopSensorThread();
}

bool PlatformSensorProviderWin::StartSensorThread() {
  if (!sensor_thread_) {
    sensor_thread_ = base::MakeUnique<base::Thread>("Sensor thread");
    sensor_thread_->init_com_with_mta(true);
  }

  if (!sensor_thread_->IsRunning())
    return sensor_thread_->Start();
  return true;
}

void PlatformSensorProviderWin::StopSensorThread() {
  if (sensor_thread_ && sensor_thread_->IsRunning()) {
    sensor_manager_.Release();
    sensor_thread_->Stop();
  }
}

void PlatformSensorProviderWin::SensorReaderCreated(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback,
    std::unique_ptr<PlatformSensorReaderWin> sensor_reader) {
  DCHECK(CalledOnValidThread());
  if (!sensor_reader) {
    callback.Run(nullptr);
    if (!HasSensors())
      StopSensorThread();
    return;
  }

  scoped_refptr<PlatformSensor> sensor = new PlatformSensorWin(
      type, std::move(mapping), this, sensor_thread_->task_runner(),
      std::move(sensor_reader));
  callback.Run(sensor);
}

std::unique_ptr<PlatformSensorReaderWin>
PlatformSensorProviderWin::CreateSensorReader(mojom::SensorType type) {
  DCHECK(sensor_thread_->task_runner()->BelongsToCurrentThread());
  if (!InitializeSensorManager())
    return nullptr;
  return PlatformSensorReaderWin::Create(type, sensor_manager_);
}

}  // namespace device
