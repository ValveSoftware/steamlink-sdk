// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/data_fetcher_shared_memory.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "content/browser/device_sensors/ambient_light_mac.h"
#include "third_party/sudden_motion_sensor/sudden_motion_sensor_mac.h"

namespace {

const double kMeanGravity = 9.80665;

double LMUvalueToLux(uint64_t raw_value) {
  // Conversion formula from regression.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=793728
  // Let x = raw_value, then
  // lux = -2.978303814*(10^-27)*x^4 + 2.635687683*(10^-19)*x^3 -
  //       3.459747434*(10^-12)*x^2 + 3.905829689*(10^-5)*x - 0.1932594532

  static const long double k4 = pow(10.L, -7);
  static const long double k3 = pow(10.L, -4);
  static const long double k2 = pow(10.L, -2);
  static const long double k1 = pow(10.L, 5);
  long double scaled_value = raw_value / k1;

  long double lux_value =
      (-3 * k4 * pow(scaled_value, 4)) + (2.6 * k3 * pow(scaled_value, 3)) +
      (-3.4 * k2 * pow(scaled_value, 2)) + (3.9 * scaled_value) - 0.19;

  double lux = ceil(static_cast<double>(lux_value));
  return lux > 0 ? lux : 0;
}

void FetchLight(content::AmbientLightSensor* sensor,
                content::DeviceLightHardwareBuffer* buffer) {
  DCHECK(sensor);
  DCHECK(buffer);
  // Macbook pro has 2 lux values, left and right, we take the average.
  // The raw sensor values are converted to lux using LMUvalueToLux(raw_value)
  // similar to how it is done in Firefox.
  uint64_t lux_value[2];
  if (!sensor->ReadSensorValue(lux_value))
    return;
  uint64_t mean = (lux_value[0] + lux_value[1]) / 2;
  double lux = LMUvalueToLux(mean);
  buffer->seqlock.WriteBegin();
  buffer->data.value = lux;
  buffer->seqlock.WriteEnd();
}

void FetchMotion(SuddenMotionSensor* sensor,
    content::DeviceMotionHardwareBuffer* buffer) {
  DCHECK(sensor);
  DCHECK(buffer);

  float axis_value[3];
  if (!sensor->ReadSensorValues(axis_value))
    return;

  buffer->seqlock.WriteBegin();
  buffer->data.accelerationIncludingGravityX = axis_value[0] * kMeanGravity;
  buffer->data.hasAccelerationIncludingGravityX = true;
  buffer->data.accelerationIncludingGravityY = axis_value[1] * kMeanGravity;
  buffer->data.hasAccelerationIncludingGravityY = true;
  buffer->data.accelerationIncludingGravityZ = axis_value[2] * kMeanGravity;
  buffer->data.hasAccelerationIncludingGravityZ = true;
  buffer->data.allAvailableSensorsAreActive = true;
  buffer->seqlock.WriteEnd();
}

void FetchOrientation(SuddenMotionSensor* sensor,
    content::DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(sensor);
  DCHECK(buffer);

  // Retrieve per-axis calibrated values.
  float axis_value[3];
  if (!sensor->ReadSensorValues(axis_value))
    return;

  // Transform the accelerometer values to W3C draft angles.
  //
  // Accelerometer values are just dot products of the sensor axes
  // by the gravity vector 'g' with the result for the z axis inverted.
  //
  // To understand this transformation calculate the 3rd row of the z-x-y
  // Euler angles rotation matrix (because of the 'g' vector, only 3rd row
  // affects to the result). Note that z-x-y matrix means R = Ry * Rx * Rz.
  // Then, assume alpha = 0 and you get this:
  //
  // x_acc = sin(gamma)
  // y_acc = - cos(gamma) * sin(beta)
  // z_acc = cos(beta) * cos(gamma)
  //
  // After that the rest is just a bit of trigonometry.
  //
  // Also note that alpha can't be provided but it's assumed to be always zero.
  // This is necessary in order to provide enough information to solve
  // the equations.
  //
  const double kRad2deg = 180.0 / M_PI;
  double beta = kRad2deg * atan2(-axis_value[1], axis_value[2]);
  double gamma = kRad2deg * asin(axis_value[0]);

  // Make sure that the interval boundaries comply with the specification. At
  // this point, beta is [-180, 180] and gamma is [-90, 90], but the spec has
  // the upper bound open on both.
  if (beta == 180.0)
    beta = -180;  // -180 == 180 (upside-down)
  if (gamma == 90.0)
    gamma = nextafter(90, 0);

  // At this point, DCHECKing is paranoia. Never hurts.
  DCHECK_GE(beta, -180.0);
  DCHECK_LT(beta,  180.0);
  DCHECK_GE(gamma, -90.0);
  DCHECK_LT(gamma,  90.0);

  buffer->seqlock.WriteBegin();
  buffer->data.beta = beta;
  buffer->data.hasBeta = true;
  buffer->data.gamma = gamma;
  buffer->data.hasGamma = true;
  buffer->data.allAvailableSensorsAreActive = true;
  buffer->seqlock.WriteEnd();
}

}  // namespace

namespace content {

DataFetcherSharedMemory::DataFetcherSharedMemory() {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

void DataFetcherSharedMemory::Fetch(unsigned consumer_bitmask) {
  DCHECK(base::MessageLoop::current() == GetPollingMessageLoop());
  DCHECK(consumer_bitmask & CONSUMER_TYPE_ORIENTATION ||
         consumer_bitmask & CONSUMER_TYPE_MOTION ||
         consumer_bitmask & CONSUMER_TYPE_LIGHT);

  if (consumer_bitmask & CONSUMER_TYPE_ORIENTATION)
    FetchOrientation(sudden_motion_sensor_.get(), orientation_buffer_);
  if (consumer_bitmask & CONSUMER_TYPE_MOTION)
    FetchMotion(sudden_motion_sensor_.get(), motion_buffer_);
  if (consumer_bitmask & CONSUMER_TYPE_LIGHT)
    FetchLight(ambient_light_sensor_.get(), light_buffer_);
}

DataFetcherSharedMemory::FetcherType DataFetcherSharedMemory::GetType() const {
  return FETCHER_TYPE_POLLING_CALLBACK;
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(base::MessageLoop::current() == GetPollingMessageLoop());
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION: {
      if (!sudden_motion_sensor_)
        sudden_motion_sensor_.reset(SuddenMotionSensor::Create());
      bool sudden_motion_sensor_available =
          sudden_motion_sensor_.get() != nullptr;

      motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.MotionMacAvailable",
          sudden_motion_sensor_available);
      if (!sudden_motion_sensor_available) {
        // No motion sensor available, fire an all-null event.
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.allAvailableSensorsAreActive = true;
        motion_buffer_->seqlock.WriteEnd();
      }
      return sudden_motion_sensor_available;
    }
    case CONSUMER_TYPE_ORIENTATION: {
      if (!sudden_motion_sensor_)
        sudden_motion_sensor_.reset(SuddenMotionSensor::Create());
      bool sudden_motion_sensor_available =
          sudden_motion_sensor_.get() != nullptr;

      orientation_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.OrientationMacAvailable",
          sudden_motion_sensor_available);
      if (sudden_motion_sensor_available) {
        // On Mac we cannot provide absolute orientation.
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.absolute = false;
        orientation_buffer_->seqlock.WriteEnd();
      } else {
        // No motion sensor available, fire an all-null event.
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.allAvailableSensorsAreActive = true;
        orientation_buffer_->seqlock.WriteEnd();
      }
      return sudden_motion_sensor_available;
    }
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE: {
      orientation_absolute_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      // Absolute device orientation not available on Mac, let the
      // implementation fire an all-null event to signal this to blink.
      orientation_absolute_buffer_->seqlock.WriteBegin();
      orientation_absolute_buffer_->data.absolute = true;
      orientation_absolute_buffer_->data.allAvailableSensorsAreActive = true;
      orientation_absolute_buffer_->seqlock.WriteEnd();
      return false;
    }
    case CONSUMER_TYPE_LIGHT: {
      if (!ambient_light_sensor_)
        ambient_light_sensor_ = AmbientLightSensor::Create();
      bool ambient_light_sensor_available =
          ambient_light_sensor_.get() != nullptr;

      light_buffer_ = static_cast<DeviceLightHardwareBuffer*>(buffer);
      if (!ambient_light_sensor_available) {
        light_buffer_->seqlock.WriteBegin();
        light_buffer_->data.value = std::numeric_limits<double>::infinity();
        light_buffer_->seqlock.WriteEnd();
      }
      return ambient_light_sensor_available;
    }
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  DCHECK(base::MessageLoop::current() == GetPollingMessageLoop());

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      if (motion_buffer_) {
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.allAvailableSensorsAreActive = false;
        motion_buffer_->seqlock.WriteEnd();
        motion_buffer_ = nullptr;
      }
      return true;
    case CONSUMER_TYPE_ORIENTATION:
      if (orientation_buffer_) {
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.allAvailableSensorsAreActive = false;
        orientation_buffer_->seqlock.WriteEnd();
        orientation_buffer_ = nullptr;
      }
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      if (orientation_absolute_buffer_) {
        orientation_absolute_buffer_->seqlock.WriteBegin();
        orientation_absolute_buffer_->data.allAvailableSensorsAreActive = false;
        orientation_absolute_buffer_->seqlock.WriteEnd();
        orientation_absolute_buffer_ = nullptr;
      }
      return true;
    case CONSUMER_TYPE_LIGHT:
      if (light_buffer_) {
        light_buffer_->seqlock.WriteBegin();
        light_buffer_->data.value = -1;
        light_buffer_->seqlock.WriteEnd();
        light_buffer_ = nullptr;
      }
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace content
