// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_PROVIDER_H_
#define DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_PROVIDER_H_

#include "device/generic_sensor/platform_sensor_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class FakePlatformSensorProvider : public PlatformSensorProvider {
 public:
  FakePlatformSensorProvider();
  ~FakePlatformSensorProvider() override;

  MOCK_METHOD0(AllSensorsRemoved, void());

 protected:
  void CreateSensorInternal(mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePlatformSensorProvider);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_MOCK_PLATFORM_SENSOR_PROVIDER_H_
