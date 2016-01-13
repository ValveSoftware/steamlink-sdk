// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/sensor_manager_android.h"

#include <string.h>

#include "base/android/jni_android.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "content/browser/device_sensors/inertial_sensor_consts.h"
#include "jni/DeviceSensors_jni.h"

using base::android::AttachCurrentThread;

namespace {

static void updateRotationVectorHistogram(bool value) {
  UMA_HISTOGRAM_BOOLEAN("InertialSensor.RotationVectorAndroidAvailable", value);
}

}

namespace content {

SensorManagerAndroid::SensorManagerAndroid()
    : number_active_device_motion_sensors_(0),
      device_motion_buffer_(NULL),
      device_orientation_buffer_(NULL),
      is_motion_buffer_ready_(false),
      is_orientation_buffer_ready_(false) {
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  device_orientation_.Reset(
      Java_DeviceSensors_getInstance(
          AttachCurrentThread(),
          base::android::GetApplicationContext()));
}

SensorManagerAndroid::~SensorManagerAndroid() {
}

bool SensorManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

SensorManagerAndroid* SensorManagerAndroid::GetInstance() {
  return Singleton<SensorManagerAndroid,
                   LeakySingletonTraits<SensorManagerAndroid> >::get();
}

void SensorManagerAndroid::GotOrientation(
    JNIEnv*, jobject, double alpha, double beta, double gamma) {
  base::AutoLock autolock(orientation_buffer_lock_);

  if (!device_orientation_buffer_)
    return;

  device_orientation_buffer_->seqlock.WriteBegin();
  device_orientation_buffer_->data.alpha = alpha;
  device_orientation_buffer_->data.hasAlpha = true;
  device_orientation_buffer_->data.beta = beta;
  device_orientation_buffer_->data.hasBeta = true;
  device_orientation_buffer_->data.gamma = gamma;
  device_orientation_buffer_->data.hasGamma = true;
  device_orientation_buffer_->seqlock.WriteEnd();

  if (!is_orientation_buffer_ready_) {
    SetOrientationBufferReadyStatus(true);
    updateRotationVectorHistogram(true);
  }
}

void SensorManagerAndroid::GotAcceleration(
    JNIEnv*, jobject, double x, double y, double z) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.accelerationX = x;
  device_motion_buffer_->data.hasAccelerationX = true;
  device_motion_buffer_->data.accelerationY = y;
  device_motion_buffer_->data.hasAccelerationY = true;
  device_motion_buffer_->data.accelerationZ = z;
  device_motion_buffer_->data.hasAccelerationZ = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!is_motion_buffer_ready_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::GotAccelerationIncludingGravity(
    JNIEnv*, jobject, double x, double y, double z) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.accelerationIncludingGravityX = x;
  device_motion_buffer_->data.hasAccelerationIncludingGravityX = true;
  device_motion_buffer_->data.accelerationIncludingGravityY = y;
  device_motion_buffer_->data.hasAccelerationIncludingGravityY = true;
  device_motion_buffer_->data.accelerationIncludingGravityZ = z;
  device_motion_buffer_->data.hasAccelerationIncludingGravityZ = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!is_motion_buffer_ready_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::GotRotationRate(
    JNIEnv*, jobject, double alpha, double beta, double gamma) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.rotationRateAlpha = alpha;
  device_motion_buffer_->data.hasRotationRateAlpha = true;
  device_motion_buffer_->data.rotationRateBeta = beta;
  device_motion_buffer_->data.hasRotationRateBeta = true;
  device_motion_buffer_->data.rotationRateGamma = gamma;
  device_motion_buffer_->data.hasRotationRateGamma = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!is_motion_buffer_ready_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] = 1;
    CheckMotionBufferReadyToRead();
  }
}

bool SensorManagerAndroid::Start(EventType event_type) {
  DCHECK(!device_orientation_.is_null());
  return Java_DeviceSensors_start(
      AttachCurrentThread(), device_orientation_.obj(),
      reinterpret_cast<intptr_t>(this), static_cast<jint>(event_type),
      kInertialSensorIntervalMillis);
}

void SensorManagerAndroid::Stop(EventType event_type) {
  DCHECK(!device_orientation_.is_null());
  Java_DeviceSensors_stop(
      AttachCurrentThread(), device_orientation_.obj(),
      static_cast<jint>(event_type));
}

int SensorManagerAndroid::GetNumberActiveDeviceMotionSensors() {
  DCHECK(!device_orientation_.is_null());
  return Java_DeviceSensors_getNumberActiveDeviceMotionSensors(
      AttachCurrentThread(), device_orientation_.obj());
}


// ----- Shared memory API methods

// --- Device Motion

bool SensorManagerAndroid::StartFetchingDeviceMotionData(
    DeviceMotionHardwareBuffer* buffer) {
  DCHECK(buffer);
  {
    base::AutoLock autolock(motion_buffer_lock_);
    device_motion_buffer_ = buffer;
    ClearInternalMotionBuffers();
  }
  bool success = Start(kTypeMotion);

  // If no motion data can ever be provided, the number of active device motion
  // sensors will be zero. In that case flag the shared memory buffer
  // as ready to read, as it will not change anyway.
  number_active_device_motion_sensors_ = GetNumberActiveDeviceMotionSensors();
  {
    base::AutoLock autolock(motion_buffer_lock_);
    CheckMotionBufferReadyToRead();
  }
  return success;
}

void SensorManagerAndroid::StopFetchingDeviceMotionData() {
  Stop(kTypeMotion);
  {
    base::AutoLock autolock(motion_buffer_lock_);
    if (device_motion_buffer_) {
      ClearInternalMotionBuffers();
      device_motion_buffer_ = NULL;
    }
  }
}

void SensorManagerAndroid::CheckMotionBufferReadyToRead() {
  if (received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] +
      received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] +
      received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] ==
      number_active_device_motion_sensors_) {
    device_motion_buffer_->seqlock.WriteBegin();
    device_motion_buffer_->data.interval = kInertialSensorIntervalMillis;
    device_motion_buffer_->seqlock.WriteEnd();
    SetMotionBufferReadyStatus(true);

    UMA_HISTOGRAM_BOOLEAN("InertialSensor.AccelerometerAndroidAvailable",
        received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] > 0);
    UMA_HISTOGRAM_BOOLEAN(
        "InertialSensor.AccelerometerIncGravityAndroidAvailable",
        received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY]
        > 0);
    UMA_HISTOGRAM_BOOLEAN("InertialSensor.GyroscopeAndroidAvailable",
        received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] > 0);
  }
}

void SensorManagerAndroid::SetMotionBufferReadyStatus(bool ready) {
  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.allAvailableSensorsAreActive = ready;
  device_motion_buffer_->seqlock.WriteEnd();
  is_motion_buffer_ready_ = ready;
}

void SensorManagerAndroid::ClearInternalMotionBuffers() {
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  number_active_device_motion_sensors_ = 0;
  SetMotionBufferReadyStatus(false);
}

// --- Device Orientation

void SensorManagerAndroid::SetOrientationBufferReadyStatus(bool ready) {
  device_orientation_buffer_->seqlock.WriteBegin();
  device_orientation_buffer_->data.absolute = ready;
  device_orientation_buffer_->data.hasAbsolute = ready;
  device_orientation_buffer_->data.allAvailableSensorsAreActive = ready;
  device_orientation_buffer_->seqlock.WriteEnd();
  is_orientation_buffer_ready_ = ready;
}

bool SensorManagerAndroid::StartFetchingDeviceOrientationData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(buffer);
  {
    base::AutoLock autolock(orientation_buffer_lock_);
    device_orientation_buffer_ = buffer;
  }
  bool success = Start(kTypeOrientation);

  {
    base::AutoLock autolock(orientation_buffer_lock_);
    // If Start() was unsuccessful then set the buffer ready flag to true
    // to start firing all-null events.
    SetOrientationBufferReadyStatus(!success);
  }

  if (!success)
    updateRotationVectorHistogram(false);

  return success;
}

void SensorManagerAndroid::StopFetchingDeviceOrientationData() {
  Stop(kTypeOrientation);
  {
    base::AutoLock autolock(orientation_buffer_lock_);
    if (device_orientation_buffer_) {
      SetOrientationBufferReadyStatus(false);
      device_orientation_buffer_ = NULL;
    }
  }
}

}  // namespace content
