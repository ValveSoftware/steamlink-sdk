// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_

#include <SensorsApi.h>

#include "base/win/scoped_comptr.h"
#include "device/generic_sensor/generic_sensor_export.h"
#include "device/generic_sensor/platform_sensor_provider.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
class Thread;
}

namespace device {

class PlatformSensorReaderWin;

// Implementation of PlatformSensorProvider for Windows platform.
// PlatformSensorProviderWin is responsible for following tasks:
// - Starts sensor thread and stops it when there are no active sensors.
// - Initialises ISensorManager and creates sensor reader on sensor thread.
// - Constructs PlatformSensorWin on IPC thread and returns it to requester.
class DEVICE_GENERIC_SENSOR_EXPORT PlatformSensorProviderWin final
    : public PlatformSensorProvider {
 public:
  static PlatformSensorProviderWin* GetInstance();

  // Overrides ISensorManager COM interface provided by the system, used
  // only for testing purposes.
  void SetSensorManagerForTesting(
      base::win::ScopedComPtr<ISensorManager> sensor_manager);

 protected:
  ~PlatformSensorProviderWin() override;

  // PlatformSensorProvider interface implementation.
  void AllSensorsRemoved() override;
  void CreateSensorInternal(mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override;

 private:
  PlatformSensorProviderWin();

  bool InitializeSensorManager();
  bool StartSensorThread();
  void StopSensorThread();
  std::unique_ptr<PlatformSensorReaderWin> CreateSensorReader(
      mojom::SensorType type);
  void SensorReaderCreated(
      mojom::SensorType type,
      mojo::ScopedSharedBufferMapping mapping,
      const CreateSensorCallback& callback,
      std::unique_ptr<PlatformSensorReaderWin> sensor_reader);

 private:
  friend struct base::DefaultSingletonTraits<PlatformSensorProviderWin>;

  base::win::ScopedComPtr<ISensorManager> sensor_manager_;
  std::unique_ptr<base::Thread> sensor_thread_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderWin);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_
