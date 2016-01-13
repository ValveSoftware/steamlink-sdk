// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/sensor_manager_android.h"

#include "base/android/jni_android.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/device_sensors/inertial_sensor_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FakeSensorManagerAndroid : public SensorManagerAndroid {
 public:
  FakeSensorManagerAndroid() { }
  virtual ~FakeSensorManagerAndroid() { }

  virtual int GetNumberActiveDeviceMotionSensors() OVERRIDE {
    return number_active_sensors_;
  }

  void SetNumberActiveDeviceMotionSensors(int number_active_sensors) {
    number_active_sensors_ = number_active_sensors;
  }

 protected:
  virtual bool Start(EventType event_type) OVERRIDE {
    return true;
  }

  virtual void Stop(EventType event_type) OVERRIDE {
  }

 private:
  int number_active_sensors_;
};

class AndroidSensorManagerTest : public testing::Test {
 protected:
  AndroidSensorManagerTest() {
    motion_buffer_.reset(new DeviceMotionHardwareBuffer);
    orientation_buffer_.reset(new DeviceOrientationHardwareBuffer);
  }

  scoped_ptr<DeviceMotionHardwareBuffer> motion_buffer_;
  scoped_ptr<DeviceOrientationHardwareBuffer> orientation_buffer_;
};

TEST_F(AndroidSensorManagerTest, ThreeDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(3);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotAcceleration(0, 0, 1, 2, 3);
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(1, motion_buffer_->data.accelerationX);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationX);
  ASSERT_EQ(2, motion_buffer_->data.accelerationY);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationY);
  ASSERT_EQ(3, motion_buffer_->data.accelerationZ);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationZ);

  sensorManager.GotAccelerationIncludingGravity(0, 0, 4, 5, 6);
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(4, motion_buffer_->data.accelerationIncludingGravityX);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationIncludingGravityX);
  ASSERT_EQ(5, motion_buffer_->data.accelerationIncludingGravityY);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationIncludingGravityY);
  ASSERT_EQ(6, motion_buffer_->data.accelerationIncludingGravityZ);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationIncludingGravityZ);

  sensorManager.GotRotationRate(0, 0, 7, 8, 9);
  ASSERT_TRUE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(7, motion_buffer_->data.rotationRateAlpha);
  ASSERT_TRUE(motion_buffer_->data.hasRotationRateAlpha);
  ASSERT_EQ(8, motion_buffer_->data.rotationRateBeta);
  ASSERT_TRUE(motion_buffer_->data.hasRotationRateBeta);
  ASSERT_EQ(9, motion_buffer_->data.rotationRateGamma);
  ASSERT_TRUE(motion_buffer_->data.hasRotationRateGamma);
  ASSERT_EQ(kInertialSensorIntervalMillis, motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidSensorManagerTest, TwoDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(2);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotAcceleration(0, 0, 1, 2, 3);
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotAccelerationIncludingGravity(0, 0, 1, 2, 3);
  ASSERT_TRUE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(kInertialSensorIntervalMillis, motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidSensorManagerTest, ZeroDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(0);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_TRUE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(kInertialSensorIntervalMillis, motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidSensorManagerTest, DeviceOrientationSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;

  sensorManager.StartFetchingDeviceOrientationData(orientation_buffer_.get());
  ASSERT_FALSE(orientation_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotOrientation(0, 0, 1, 2, 3);
  ASSERT_TRUE(orientation_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(1, orientation_buffer_->data.alpha);
  ASSERT_TRUE(orientation_buffer_->data.hasAlpha);
  ASSERT_EQ(2, orientation_buffer_->data.beta);
  ASSERT_TRUE(orientation_buffer_->data.hasBeta);
  ASSERT_EQ(3, orientation_buffer_->data.gamma);
  ASSERT_TRUE(orientation_buffer_->data.hasGamma);

  sensorManager.StopFetchingDeviceOrientationData();
  ASSERT_FALSE(orientation_buffer_->data.allAvailableSensorsAreActive);
}


}  // namespace

}  // namespace content
