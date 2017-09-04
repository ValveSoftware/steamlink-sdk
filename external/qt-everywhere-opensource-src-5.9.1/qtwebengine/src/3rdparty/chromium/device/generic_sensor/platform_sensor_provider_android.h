// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_ANDROID_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_ANDROID_H_

#include "device/generic_sensor/platform_sensor_provider.h"

#include "base/android/scoped_java_ref.h"

namespace device {

class PlatformSensorProviderAndroid : public PlatformSensorProvider {
 public:
  PlatformSensorProviderAndroid();
  ~PlatformSensorProviderAndroid() override;

  static PlatformSensorProviderAndroid* GetInstance();

 protected:
  void CreateSensorInternal(mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override;

 private:
  // Java object org.chromium.device.sensors.PlatformSensorProvider
  base::android::ScopedJavaGlobalRef<jobject> j_object_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderAndroid);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_ANDROID_H_
