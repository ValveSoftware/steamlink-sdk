// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_H_

#include "content/browser/device_sensors/data_fetcher_shared_memory_base.h"

#if !defined(OS_ANDROID)
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

class CONTENT_EXPORT DataFetcherSharedMemory
    : public DataFetcherSharedMemoryBase {

 public:
  DataFetcherSharedMemory();
  virtual ~DataFetcherSharedMemory();

 private:
  virtual bool Start(ConsumerType consumer_type, void* buffer) OVERRIDE;
  virtual bool Stop(ConsumerType consumer_type) OVERRIDE;

#if !defined(OS_ANDROID)
  DeviceMotionHardwareBuffer* motion_buffer_;
  DeviceOrientationHardwareBuffer* orientation_buffer_;
#endif
#if defined(OS_MACOSX)
  virtual void Fetch(unsigned consumer_bitmask) OVERRIDE;
  virtual FetcherType GetType() const OVERRIDE;

  scoped_ptr<SuddenMotionSensor> sudden_motion_sensor_;
#elif defined(OS_WIN)
  class SensorEventSink;
  class SensorEventSinkMotion;
  class SensorEventSinkOrientation;

  virtual FetcherType GetType() const OVERRIDE;

  bool RegisterForSensor(REFSENSOR_TYPE_ID sensor_type, ISensor** sensor,
      scoped_refptr<SensorEventSink> event_sink);
  void DisableSensors(ConsumerType consumer_type);
  void SetBufferAvailableState(ConsumerType consumer_type, bool enabled);

  base::win::ScopedComPtr<ISensor> sensor_inclinometer_;
  base::win::ScopedComPtr<ISensor> sensor_accelerometer_;
  base::win::ScopedComPtr<ISensor> sensor_gyrometer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DataFetcherSharedMemory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_DATA_FETCHER_SHARED_MEMORY_H_
