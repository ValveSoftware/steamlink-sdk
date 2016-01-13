// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"

template<typename T> struct DefaultSingletonTraits;

namespace content {

// Android implementation of Device Orientation API.
//
// Android's SensorManager has a push API, so when Got*() methods are called
// by the system the browser process puts the received data into a shared
// memory buffer, which is read by the renderer processes.
class CONTENT_EXPORT SensorManagerAndroid {
 public:
  // Must be called at startup, before GetInstance().
  static bool Register(JNIEnv* env);

  // Needs to be thread-safe, because accessed from different threads.
  static SensorManagerAndroid* GetInstance();

  // Called from Java via JNI.
  void GotOrientation(JNIEnv*, jobject,
                      double alpha, double beta, double gamma);
  void GotAcceleration(JNIEnv*, jobject,
                       double x, double y, double z);
  void GotAccelerationIncludingGravity(JNIEnv*, jobject,
                                       double x, double y, double z);
  void GotRotationRate(JNIEnv*, jobject,
                       double alpha, double beta, double gamma);

  // Shared memory related methods.
  bool StartFetchingDeviceMotionData(DeviceMotionHardwareBuffer* buffer);
  void StopFetchingDeviceMotionData();

  bool StartFetchingDeviceOrientationData(
      DeviceOrientationHardwareBuffer* buffer);
  void StopFetchingDeviceOrientationData();

 protected:
  enum EventType {
    // These constants should match DEVICE_ORIENTATION and DEVICE_MOTION
    // constants in content/public/android/java/src/org/chromium/content/
    // browser/DeviceMotionAndOrientation.java
    kTypeOrientation = 0,
    kTypeMotion = 1
  };

  SensorManagerAndroid();
  virtual ~SensorManagerAndroid();

  virtual bool Start(EventType event_type);
  virtual void Stop(EventType event_type);
  virtual int GetNumberActiveDeviceMotionSensors();

 private:
  friend struct DefaultSingletonTraits<SensorManagerAndroid>;

  enum {
    RECEIVED_MOTION_DATA_ACCELERATION = 0,
    RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY = 1,
    RECEIVED_MOTION_DATA_ROTATION_RATE = 2,
    RECEIVED_MOTION_DATA_MAX = 3,
  };

  void CheckMotionBufferReadyToRead();
  void SetMotionBufferReadyStatus(bool ready);
  void ClearInternalMotionBuffers();

  void SetOrientationBufferReadyStatus(bool ready);

  // The Java provider of orientation info.
  base::android::ScopedJavaGlobalRef<jobject> device_orientation_;
  int number_active_device_motion_sensors_;
  int received_motion_data_[RECEIVED_MOTION_DATA_MAX];
  DeviceMotionHardwareBuffer* device_motion_buffer_;
  DeviceOrientationHardwareBuffer* device_orientation_buffer_;
  bool is_motion_buffer_ready_;
  bool is_orientation_buffer_ready_;

  base::Lock motion_buffer_lock_;
  base::Lock orientation_buffer_lock_;

  DISALLOW_COPY_AND_ASSIGN(SensorManagerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_ANDROID_H_
