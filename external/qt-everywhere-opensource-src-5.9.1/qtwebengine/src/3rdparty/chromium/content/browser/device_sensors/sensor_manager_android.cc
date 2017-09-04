// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/sensor_manager_android.h"

#include <string.h>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "jni/DeviceSensors_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {

void UpdateDeviceOrientationHistogram(
    content::SensorManagerAndroid::OrientationSensorType type) {
  UMA_HISTOGRAM_ENUMERATION("InertialSensor.DeviceOrientationSensorAndroid",
      type, content::SensorManagerAndroid::ORIENTATION_SENSOR_MAX);
}

void SetOrientation(content::DeviceOrientationHardwareBuffer* buffer,
    double alpha, double beta, double gamma) {
  buffer->seqlock.WriteBegin();
  buffer->data.alpha = alpha;
  buffer->data.hasAlpha = true;
  buffer->data.beta = beta;
  buffer->data.hasBeta = true;
  buffer->data.gamma = gamma;
  buffer->data.hasGamma = true;
  buffer->seqlock.WriteEnd();
}

void SetOrientationBufferStatus(
    content::DeviceOrientationHardwareBuffer* buffer,
    bool ready, bool absolute) {
  buffer->seqlock.WriteBegin();
  buffer->data.absolute = absolute;
  buffer->data.allAvailableSensorsAreActive = ready;
  buffer->seqlock.WriteEnd();
}

}  // namespace

namespace content {

SensorManagerAndroid::SensorManagerAndroid()
    : number_active_device_motion_sensors_(0),
      device_light_buffer_(nullptr),
      device_motion_buffer_(nullptr),
      device_orientation_buffer_(nullptr),
      motion_buffer_initialized_(false),
      orientation_buffer_initialized_(false),
      is_shutdown_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  device_sensors_.Reset(Java_DeviceSensors_getInstance(
      AttachCurrentThread(), base::android::GetApplicationContext()));
}

SensorManagerAndroid::~SensorManagerAndroid() {
}

bool SensorManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

SensorManagerAndroid* SensorManagerAndroid::GetInstance() {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  return base::Singleton<
      SensorManagerAndroid,
      base::LeakySingletonTraits<SensorManagerAndroid>>::get();
}

void SensorManagerAndroid::GotOrientation(JNIEnv*,
                                          const JavaParamRef<jobject>&,
                                          double alpha,
                                          double beta,
                                          double gamma) {
  base::AutoLock autolock(orientation_buffer_lock_);

  if (!device_orientation_buffer_)
    return;

  SetOrientation(device_orientation_buffer_, alpha, beta, gamma);

  if (!orientation_buffer_initialized_) {
    OrientationSensorType type =
        static_cast<OrientationSensorType>(GetOrientationSensorTypeUsed());
    SetOrientationBufferStatus(device_orientation_buffer_, true,
        type != GAME_ROTATION_VECTOR);
    orientation_buffer_initialized_ = true;
    UpdateDeviceOrientationHistogram(type);
  }
}

void SensorManagerAndroid::GotOrientationAbsolute(JNIEnv*,
                                                  const JavaParamRef<jobject>&,
                                                  double alpha,
                                                  double beta,
                                                  double gamma) {
  base::AutoLock autolock(orientation_absolute_buffer_lock_);

  if (!device_orientation_absolute_buffer_)
    return;

  SetOrientation(device_orientation_absolute_buffer_, alpha, beta, gamma);

  if (!orientation_absolute_buffer_initialized_) {
    SetOrientationBufferStatus(device_orientation_absolute_buffer_, true, true);
    orientation_absolute_buffer_initialized_ = true;
    // TODO(timvolodine): Add UMA.
  }
}

void SensorManagerAndroid::GotAcceleration(JNIEnv*,
                                           const JavaParamRef<jobject>&,
                                           double x,
                                           double y,
                                           double z) {
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

  if (!motion_buffer_initialized_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::GotAccelerationIncludingGravity(
    JNIEnv*,
    const JavaParamRef<jobject>&,
    double x,
    double y,
    double z) {
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

  if (!motion_buffer_initialized_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::GotRotationRate(JNIEnv*,
                                           const JavaParamRef<jobject>&,
                                           double alpha,
                                           double beta,
                                           double gamma) {
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

  if (!motion_buffer_initialized_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::GotLight(JNIEnv*,
                                    const JavaParamRef<jobject>&,
                                    double value) {
  base::AutoLock autolock(light_buffer_lock_);

  if (!device_light_buffer_)
    return;

  device_light_buffer_->seqlock.WriteBegin();
  device_light_buffer_->data.value = value;
  device_light_buffer_->seqlock.WriteEnd();
}

bool SensorManagerAndroid::Start(ConsumerType consumer_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_sensors_.is_null());
  int rate_in_microseconds = (consumer_type == CONSUMER_TYPE_LIGHT)
                                 ? kLightSensorIntervalMicroseconds
                                 : kDeviceSensorIntervalMicroseconds;
  return Java_DeviceSensors_start(
      AttachCurrentThread(), device_sensors_, reinterpret_cast<intptr_t>(this),
      static_cast<jint>(consumer_type), rate_in_microseconds);
}

void SensorManagerAndroid::Stop(ConsumerType consumer_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_sensors_.is_null());
  Java_DeviceSensors_stop(AttachCurrentThread(), device_sensors_,
                          static_cast<jint>(consumer_type));
}

int SensorManagerAndroid::GetNumberActiveDeviceMotionSensors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_sensors_.is_null());
  return Java_DeviceSensors_getNumberActiveDeviceMotionSensors(
      AttachCurrentThread(), device_sensors_);
}

SensorManagerAndroid::OrientationSensorType
SensorManagerAndroid::GetOrientationSensorTypeUsed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_sensors_.is_null());
  return static_cast<SensorManagerAndroid::OrientationSensorType>(
      Java_DeviceSensors_getOrientationSensorTypeUsed(AttachCurrentThread(),
                                                      device_sensors_));
}

// ----- Shared memory API methods

// --- Device Light

void SensorManagerAndroid::StartFetchingDeviceLightData(
    DeviceLightHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer);
  if (is_shutdown_)
    return;

  {
    base::AutoLock autolock(light_buffer_lock_);
    device_light_buffer_ = buffer;
    SetLightBufferValue(-1);
  }
  bool success = Start(CONSUMER_TYPE_LIGHT);
  if (!success) {
    base::AutoLock autolock(light_buffer_lock_);
    SetLightBufferValue(std::numeric_limits<double>::infinity());
  }
}

void SensorManagerAndroid::StopFetchingDeviceLightData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  Stop(CONSUMER_TYPE_LIGHT);
  {
    base::AutoLock autolock(light_buffer_lock_);
    if (device_light_buffer_) {
      SetLightBufferValue(-1);
      device_light_buffer_ = nullptr;
    }
  }
}

void SensorManagerAndroid::SetLightBufferValue(double lux) {
  device_light_buffer_->seqlock.WriteBegin();
  device_light_buffer_->data.value = lux;
  device_light_buffer_->seqlock.WriteEnd();
}

// --- Device Motion

void SensorManagerAndroid::StartFetchingDeviceMotionData(
    DeviceMotionHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer);
  if (is_shutdown_)
    return;

  {
    base::AutoLock autolock(motion_buffer_lock_);
    device_motion_buffer_ = buffer;
    ClearInternalMotionBuffers();
  }
  Start(CONSUMER_TYPE_MOTION);

  // If no motion data can ever be provided, the number of active device motion
  // sensors will be zero. In that case flag the shared memory buffer
  // as ready to read, as it will not change anyway.
  number_active_device_motion_sensors_ = GetNumberActiveDeviceMotionSensors();
  {
    base::AutoLock autolock(motion_buffer_lock_);
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::StopFetchingDeviceMotionData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  Stop(CONSUMER_TYPE_MOTION);
  {
    base::AutoLock autolock(motion_buffer_lock_);
    if (device_motion_buffer_) {
      ClearInternalMotionBuffers();
      device_motion_buffer_ = nullptr;
    }
  }
}

void SensorManagerAndroid::CheckMotionBufferReadyToRead() {
  if (received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] +
      received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] +
      received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] ==
      number_active_device_motion_sensors_) {
    device_motion_buffer_->seqlock.WriteBegin();
    device_motion_buffer_->data.interval =
        kDeviceSensorIntervalMicroseconds / 1000.;
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
  motion_buffer_initialized_ = ready;
}

void SensorManagerAndroid::ClearInternalMotionBuffers() {
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  number_active_device_motion_sensors_ = 0;
  SetMotionBufferReadyStatus(false);
}

// --- Device Orientation

void SensorManagerAndroid::StartFetchingDeviceOrientationData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer);
  if (is_shutdown_)
    return;

  {
    base::AutoLock autolock(orientation_buffer_lock_);
    device_orientation_buffer_ = buffer;
  }
  bool success = Start(CONSUMER_TYPE_ORIENTATION);

  {
    base::AutoLock autolock(orientation_buffer_lock_);
    // If Start() was unsuccessful then set the buffer ready flag to true
    // to start firing all-null events.
    SetOrientationBufferStatus(buffer, !success /* ready */,
        false /* absolute */);
    orientation_buffer_initialized_ = !success;
  }

  if (!success)
    UpdateDeviceOrientationHistogram(NOT_AVAILABLE);
}

void SensorManagerAndroid::StopFetchingDeviceOrientationData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  Stop(CONSUMER_TYPE_ORIENTATION);
  {
    base::AutoLock autolock(orientation_buffer_lock_);
    if (device_orientation_buffer_) {
      SetOrientationBufferStatus(device_orientation_buffer_, false, false);
      orientation_buffer_initialized_ = false;
      device_orientation_buffer_ = nullptr;
    }
  }
}

void SensorManagerAndroid::StartFetchingDeviceOrientationAbsoluteData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer);
  if (is_shutdown_)
    return;

  {
    base::AutoLock autolock(orientation_absolute_buffer_lock_);
    device_orientation_absolute_buffer_ = buffer;
  }
  bool success = Start(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);

  {
    base::AutoLock autolock(orientation_absolute_buffer_lock_);
    // If Start() was unsuccessful then set the buffer ready flag to true
    // to start firing all-null events.
    SetOrientationBufferStatus(buffer, !success /* ready */,
        false /* absolute */);
    orientation_absolute_buffer_initialized_ = !success;
  }
}

void SensorManagerAndroid::StopFetchingDeviceOrientationAbsoluteData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  Stop(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  {
    base::AutoLock autolock(orientation_absolute_buffer_lock_);
    if (device_orientation_absolute_buffer_) {
      SetOrientationBufferStatus(device_orientation_absolute_buffer_, false,
          false);
      orientation_absolute_buffer_initialized_ = false;
      device_orientation_absolute_buffer_ = nullptr;
    }
  }
}

void SensorManagerAndroid::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_shutdown_ = true;
}

}  // namespace content
