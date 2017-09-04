// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_
#define DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "device/generic_sensor/generic_sensor_export.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom.h"

namespace device {

class PlatformSensorProvider;
class PlatformSensor;

// Implementation of SensorProvider mojo interface.
// Uses PlatformSensorProvider singleton to create platform specific instances
// of PlatformSensor which are used by SensorImpl.
class DEVICE_GENERIC_SENSOR_EXPORT SensorProviderImpl final
    : public mojom::SensorProvider {
 public:
  static void Create(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      mojom::SensorProviderRequest request);

  ~SensorProviderImpl() override;

 private:
  explicit SensorProviderImpl(PlatformSensorProvider* provider);

  // SensorProvider implementation.
  void GetSensor(mojom::SensorType type,
                 mojom::SensorRequest sensor_request,
                 const GetSensorCallback& callback) override;

  // Helper callback method to return created sensors.
  void SensorCreated(mojom::SensorType type,
                     mojo::ScopedSharedBufferHandle cloned_handle,
                     mojom::SensorRequest sensor_request,
                     const GetSensorCallback& callback,
                     scoped_refptr<PlatformSensor> sensor);

  PlatformSensorProvider* provider_;
  base::WeakPtrFactory<SensorProviderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SensorProviderImpl);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_
