// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/data_fetcher_shared_memory.h"

#include "base/logging.h"
#include "content/browser/device_sensors/sensor_manager_android.h"
#include "content/common/device_sensors/device_light_hardware_buffer.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"

namespace content {

DataFetcherSharedMemory::DataFetcherSharedMemory() {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      SensorManagerAndroid::GetInstance()->StartFetchingDeviceMotionData(
          static_cast<DeviceMotionHardwareBuffer*>(buffer));
      return true;
    case CONSUMER_TYPE_ORIENTATION:
      SensorManagerAndroid::GetInstance()->StartFetchingDeviceOrientationData(
          static_cast<DeviceOrientationHardwareBuffer*>(buffer));
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      SensorManagerAndroid::GetInstance()->
          StartFetchingDeviceOrientationAbsoluteData(
              static_cast<DeviceOrientationHardwareBuffer*>(buffer));
      return true;
    case CONSUMER_TYPE_LIGHT:
      SensorManagerAndroid::GetInstance()->StartFetchingDeviceLightData(
          static_cast<DeviceLightHardwareBuffer*>(buffer));
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      SensorManagerAndroid::GetInstance()->StopFetchingDeviceMotionData();
      return true;
    case CONSUMER_TYPE_ORIENTATION:
      SensorManagerAndroid::GetInstance()->StopFetchingDeviceOrientationData();
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      SensorManagerAndroid::GetInstance()
          ->StopFetchingDeviceOrientationAbsoluteData();
      return true;
    case CONSUMER_TYPE_LIGHT:
      SensorManagerAndroid::GetInstance()->StopFetchingDeviceLightData();
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

void DataFetcherSharedMemory::Shutdown() {
  DataFetcherSharedMemoryBase::Shutdown();
  SensorManagerAndroid::GetInstance()->Shutdown();
}

}  // namespace content
