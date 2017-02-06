// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/sensor_manager_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/device_sensors/device_sensors_consts.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FakeSensorManagerAndroid : public SensorManagerAndroid {
 public:
  FakeSensorManagerAndroid() {}
  ~FakeSensorManagerAndroid() override {}

  OrientationSensorType GetOrientationSensorTypeUsed() override {
    return SensorManagerAndroid::ROTATION_VECTOR;
  }

  int GetNumberActiveDeviceMotionSensors() override {
    return number_active_sensors_;
  }

  void SetNumberActiveDeviceMotionSensors(int number_active_sensors) {
    number_active_sensors_ = number_active_sensors;
  }

 protected:
  bool Start(ConsumerType event_type) override { return true; }
  void Stop(ConsumerType event_type) override {}

 private:
  int number_active_sensors_;
};

class AndroidSensorManagerTest : public testing::Test {
 protected:
  AndroidSensorManagerTest() {
    light_buffer_.reset(new DeviceLightHardwareBuffer);
    motion_buffer_.reset(new DeviceMotionHardwareBuffer);
    orientation_buffer_.reset(new DeviceOrientationHardwareBuffer);
    orientation_absolute_buffer_.reset(new DeviceOrientationHardwareBuffer);
  }

  void VerifyOrientationBufferValues(
      const DeviceOrientationHardwareBuffer* buffer,
      double alpha, double beta, double gamma) {
    ASSERT_TRUE(buffer->data.allAvailableSensorsAreActive);
    ASSERT_EQ(alpha, buffer->data.alpha);
    ASSERT_TRUE(buffer->data.hasAlpha);
    ASSERT_EQ(beta, buffer->data.beta);
    ASSERT_TRUE(buffer->data.hasBeta);
    ASSERT_EQ(gamma, buffer->data.gamma);
    ASSERT_TRUE(buffer->data.hasGamma);
  }

  std::unique_ptr<DeviceLightHardwareBuffer> light_buffer_;
  std::unique_ptr<DeviceMotionHardwareBuffer> motion_buffer_;
  std::unique_ptr<DeviceOrientationHardwareBuffer> orientation_buffer_;
  std::unique_ptr<DeviceOrientationHardwareBuffer> orientation_absolute_buffer_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(AndroidSensorManagerTest, ThreeDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(3);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotAcceleration(nullptr, nullptr, 1, 2, 3);
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(1, motion_buffer_->data.accelerationX);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationX);
  ASSERT_EQ(2, motion_buffer_->data.accelerationY);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationY);
  ASSERT_EQ(3, motion_buffer_->data.accelerationZ);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationZ);

  sensorManager.GotAccelerationIncludingGravity(nullptr, nullptr, 4, 5, 6);
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(4, motion_buffer_->data.accelerationIncludingGravityX);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationIncludingGravityX);
  ASSERT_EQ(5, motion_buffer_->data.accelerationIncludingGravityY);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationIncludingGravityY);
  ASSERT_EQ(6, motion_buffer_->data.accelerationIncludingGravityZ);
  ASSERT_TRUE(motion_buffer_->data.hasAccelerationIncludingGravityZ);

  sensorManager.GotRotationRate(nullptr, nullptr, 7, 8, 9);
  ASSERT_TRUE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(7, motion_buffer_->data.rotationRateAlpha);
  ASSERT_TRUE(motion_buffer_->data.hasRotationRateAlpha);
  ASSERT_EQ(8, motion_buffer_->data.rotationRateBeta);
  ASSERT_TRUE(motion_buffer_->data.hasRotationRateBeta);
  ASSERT_EQ(9, motion_buffer_->data.rotationRateGamma);
  ASSERT_TRUE(motion_buffer_->data.hasRotationRateGamma);
  ASSERT_EQ(kInertialSensorIntervalMicroseconds / 1000.,
            motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidSensorManagerTest, TwoDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(2);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotAcceleration(nullptr, nullptr, 1, 2, 3);
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotAccelerationIncludingGravity(nullptr, nullptr, 1, 2, 3);
  ASSERT_TRUE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(kInertialSensorIntervalMicroseconds / 1000.,
            motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidSensorManagerTest, ZeroDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(0);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_TRUE(motion_buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(kInertialSensorIntervalMicroseconds / 1000.,
            motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidSensorManagerTest, DeviceOrientationSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;

  sensorManager.StartFetchingDeviceOrientationData(orientation_buffer_.get());
  ASSERT_FALSE(orientation_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotOrientation(nullptr, nullptr, 1, 2, 3);
  VerifyOrientationBufferValues(orientation_buffer_.get(), 1, 2, 3);

  sensorManager.StopFetchingDeviceOrientationData();
  ASSERT_FALSE(orientation_buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidSensorManagerTest, DeviceOrientationAbsoluteSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;

  sensorManager.StartFetchingDeviceOrientationAbsoluteData(
      orientation_absolute_buffer_.get());
  ASSERT_FALSE(orientation_buffer_->data.allAvailableSensorsAreActive);

  sensorManager.GotOrientationAbsolute(nullptr, nullptr, 4, 5, 6);
  VerifyOrientationBufferValues(orientation_absolute_buffer_.get(), 4, 5, 6);

  sensorManager.StopFetchingDeviceOrientationData();
  ASSERT_FALSE(orientation_buffer_->data.allAvailableSensorsAreActive);
}

// DeviceLight
TEST_F(AndroidSensorManagerTest, DeviceLightSensorsActive) {
  FakeSensorManagerAndroid::Register(base::android::AttachCurrentThread());
  FakeSensorManagerAndroid sensorManager;

  sensorManager.StartFetchingDeviceLightData(light_buffer_.get());

  sensorManager.GotLight(nullptr, nullptr, 100);
  ASSERT_EQ(100, light_buffer_->data.value);

  sensorManager.StopFetchingDeviceLightData();
  ASSERT_EQ(-1, light_buffer_->data.value);
}

}  // namespace

}  // namespace content
