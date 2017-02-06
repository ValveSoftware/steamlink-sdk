// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/device_sensors/data_fetcher_shared_memory_base.h"

#if !defined(OS_ANDROID)
#include "content/common/device_sensors/device_light_hardware_buffer.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"
#endif

#if defined(OS_MACOSX)
class SuddenMotionSensor;
#elif defined(OS_WIN)
#include <SensorsApi.h>
#include "base/win/scoped_comptr.h"
#endif

namespace content {

#if defined(OS_CHROMEOS)
class SensorManagerChromeOS;
#elif defined(OS_MACOSX)
class AmbientLightSensor;
#endif

class CONTENT_EXPORT DataFetcherSharedMemory
    : public DataFetcherSharedMemoryBase {

 public:
  DataFetcherSharedMemory();
  ~DataFetcherSharedMemory() override;

#if defined(OS_ANDROID)
  void Shutdown() override;
#endif

 private:
  bool Start(ConsumerType consumer_type, void* buffer) override;
  bool Stop(ConsumerType consumer_type) override;

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
#if !defined(OS_CHROMEOS)
  DeviceMotionHardwareBuffer* motion_buffer_ = nullptr;
  DeviceOrientationHardwareBuffer* orientation_buffer_ = nullptr;
  DeviceLightHardwareBuffer* light_buffer_ = nullptr;
#endif
  DeviceOrientationHardwareBuffer* orientation_absolute_buffer_ = nullptr;
#endif

#if defined(OS_CHROMEOS)
  std::unique_ptr<SensorManagerChromeOS> sensor_manager_;
#elif defined(OS_MACOSX)
  void Fetch(unsigned consumer_bitmask) override;
  FetcherType GetType() const override;

  std::unique_ptr<AmbientLightSensor> ambient_light_sensor_;
  std::unique_ptr<SuddenMotionSensor> sudden_motion_sensor_;
#elif defined(OS_WIN)
  class SensorEventSink;
  class SensorEventSinkMotion;
  class SensorEventSinkOrientation;
  class SensorEventSinkLight;

  FetcherType GetType() const override;

  bool RegisterForSensor(REFSENSOR_TYPE_ID sensor_type, ISensor** sensor,
      scoped_refptr<SensorEventSink> event_sink);
  void DisableSensors(ConsumerType consumer_type);
  void SetBufferAvailableState(ConsumerType consumer_type, bool enabled);

  base::win::ScopedComPtr<ISensor> sensor_inclinometer_;
  base::win::ScopedComPtr<ISensor> sensor_inclinometer_absolute_;
  base::win::ScopedComPtr<ISensor> sensor_accelerometer_;
  base::win::ScopedComPtr<ISensor> sensor_gyrometer_;
  base::win::ScopedComPtr<ISensor> sensor_light_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DataFetcherSharedMemory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_H_
