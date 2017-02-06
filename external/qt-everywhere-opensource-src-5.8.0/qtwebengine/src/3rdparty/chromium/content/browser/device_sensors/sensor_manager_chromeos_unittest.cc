// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/sensor_manager_chromeos.h"

#include <memory>

#include "base/macros.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const double kMeanGravity = -9.80665;

// Isolated content::SensorManagerChromeOS from the active
// chromeos::AccelerometerReader. This allows for direct control over which
// accelerometer events are provided to the sensor manager.
class TestSensorManagerChromeOS : public content::SensorManagerChromeOS {
 public:
  TestSensorManagerChromeOS() {}
  ~TestSensorManagerChromeOS() override {};

 protected:
  void StartObservingAccelerometer() override {}
  void StopObservingAccelerometer() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSensorManagerChromeOS);
};

}  // namespace

namespace content {

class SensorManagerChromeOSTest : public testing::Test {
 public:
  SensorManagerChromeOSTest() {
    motion_buffer_.reset(new DeviceMotionHardwareBuffer);
    orientation_buffer_.reset(new DeviceOrientationHardwareBuffer);
  }

  ~SensorManagerChromeOSTest() override {}

  void OnAccelerationIncludingGravity(double x, double y, double z) {
    scoped_refptr<chromeos::AccelerometerUpdate> update(
        new chromeos::AccelerometerUpdate());
    update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, x, y, z);
    sensor_manager_->OnAccelerometerUpdated(update);
  }

  DeviceMotionHardwareBuffer* motion_buffer() { return motion_buffer_.get(); }

  DeviceOrientationHardwareBuffer* orientation_buffer() {
    return orientation_buffer_.get();
  }

  SensorManagerChromeOS* sensor_manager() { return sensor_manager_.get(); }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    sensor_manager_.reset(new TestSensorManagerChromeOS);
    sensor_manager_->StartFetchingDeviceMotionData(motion_buffer_.get());
    sensor_manager_->StartFetchingDeviceOrientationData(
        orientation_buffer_.get());
  }

  void TearDown() override {
    sensor_manager_->StopFetchingDeviceMotionData();
    sensor_manager_->StopFetchingDeviceOrientationData();
    testing::Test::TearDown();
  }

 private:
  std::unique_ptr<TestSensorManagerChromeOS> sensor_manager_;
  std::unique_ptr<DeviceMotionHardwareBuffer> motion_buffer_;
  std::unique_ptr<DeviceOrientationHardwareBuffer> orientation_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorManagerChromeOSTest);
};

// Tests that starting to process motion data will update the associated buffer.
TEST_F(SensorManagerChromeOSTest, MotionBuffer) {
  DeviceMotionHardwareBuffer* buffer = motion_buffer();
  EXPECT_FLOAT_EQ(100.0f, buffer->data.interval);
  EXPECT_FALSE(buffer->data.hasAccelerationIncludingGravityX);
  EXPECT_FALSE(buffer->data.hasAccelerationIncludingGravityY);
  EXPECT_FALSE(buffer->data.hasAccelerationIncludingGravityZ);
  EXPECT_FALSE(buffer->data.hasAccelerationX);
  EXPECT_FALSE(buffer->data.hasAccelerationY);
  EXPECT_FALSE(buffer->data.hasAccelerationZ);
  EXPECT_FALSE(buffer->data.hasRotationRateAlpha);
  EXPECT_FALSE(buffer->data.hasRotationRateBeta);
  EXPECT_FALSE(buffer->data.hasRotationRateGamma);

  OnAccelerationIncludingGravity(0.0f, 0.0f, 1.0f);
  EXPECT_TRUE(buffer->data.hasAccelerationIncludingGravityX);
  EXPECT_TRUE(buffer->data.hasAccelerationIncludingGravityY);
  EXPECT_TRUE(buffer->data.hasAccelerationIncludingGravityZ);
  EXPECT_FALSE(buffer->data.hasAccelerationX);
  EXPECT_FALSE(buffer->data.hasAccelerationY);
  EXPECT_FALSE(buffer->data.hasAccelerationZ);
  EXPECT_FALSE(buffer->data.hasRotationRateAlpha);
  EXPECT_FALSE(buffer->data.hasRotationRateBeta);
  EXPECT_FALSE(buffer->data.hasRotationRateGamma);
  EXPECT_TRUE(buffer->data.allAvailableSensorsAreActive);

  sensor_manager()->StopFetchingDeviceMotionData();
  EXPECT_FALSE(buffer->data.allAvailableSensorsAreActive);
}

// Tests that starting to process orientation data will update the associated
// buffer.
TEST_F(SensorManagerChromeOSTest, OrientationBuffer) {
  DeviceOrientationHardwareBuffer* buffer = orientation_buffer();
  EXPECT_FALSE(buffer->data.hasAlpha);
  EXPECT_FALSE(buffer->data.hasBeta);
  EXPECT_FALSE(buffer->data.hasGamma);
  EXPECT_FALSE(buffer->data.allAvailableSensorsAreActive);

  OnAccelerationIncludingGravity(0.0f, 0.0f, 1.0f);
  EXPECT_FLOAT_EQ(0.0f, buffer->data.alpha);
  EXPECT_FALSE(buffer->data.hasAlpha);
  EXPECT_TRUE(buffer->data.hasBeta);
  EXPECT_TRUE(buffer->data.hasGamma);
  EXPECT_TRUE(buffer->data.allAvailableSensorsAreActive);

  sensor_manager()->StopFetchingDeviceOrientationData();
  EXPECT_FALSE(buffer->data.allAvailableSensorsAreActive);
}

// Tests a device resting flat.
TEST_F(SensorManagerChromeOSTest, NeutralOrientation) {
  OnAccelerationIncludingGravity(0.0f, 0.0f, -kMeanGravity);

  DeviceMotionHardwareBuffer* motion = motion_buffer();
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityX);
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityY);
  EXPECT_FLOAT_EQ(-kMeanGravity, motion->data.accelerationIncludingGravityZ);

  DeviceOrientationHardwareBuffer* orientation = orientation_buffer();
  EXPECT_FLOAT_EQ(0.0f, orientation->data.beta);
  EXPECT_FLOAT_EQ(0.0f, orientation->data.gamma);
}

// Tests an upside-down device, such that the W3C boundary [-180,180) causes the
// beta value to become negative.
TEST_F(SensorManagerChromeOSTest, UpsideDown) {
  OnAccelerationIncludingGravity(0.0f, 0.0f, kMeanGravity);

  DeviceMotionHardwareBuffer* motion = motion_buffer();
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityX);
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityY);
  EXPECT_FLOAT_EQ(kMeanGravity, motion->data.accelerationIncludingGravityZ);

  DeviceOrientationHardwareBuffer* orientation = orientation_buffer();
  EXPECT_FLOAT_EQ(-180.0f, orientation->data.beta);
  EXPECT_FLOAT_EQ(0.0f, orientation->data.gamma);
}

// Tests for positive beta value before the device is completely upside-down
TEST_F(SensorManagerChromeOSTest, BeforeUpsideDownBoundary) {
  OnAccelerationIncludingGravity(0.0f, -kMeanGravity / 2.0f,
                                 kMeanGravity / 2.0f);

  DeviceMotionHardwareBuffer* motion = motion_buffer();
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityX);
  EXPECT_FLOAT_EQ(-kMeanGravity / 2.0f,
                  motion->data.accelerationIncludingGravityY);
  EXPECT_FLOAT_EQ(kMeanGravity / 2.0f,
                  motion->data.accelerationIncludingGravityZ);

  DeviceOrientationHardwareBuffer* orientation = orientation_buffer();
  EXPECT_FLOAT_EQ(135.0f, orientation->data.beta);
  EXPECT_FLOAT_EQ(0.0f, orientation->data.gamma);
}

// Tests a device lying on its left-edge.
TEST_F(SensorManagerChromeOSTest, LeftEdge) {
  OnAccelerationIncludingGravity(-kMeanGravity, 0.0f, 0.0f);

  DeviceMotionHardwareBuffer* motion = motion_buffer();
  EXPECT_FLOAT_EQ(-kMeanGravity, motion->data.accelerationIncludingGravityX);
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityY);
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityZ);

  DeviceOrientationHardwareBuffer* orientation = orientation_buffer();
  EXPECT_FLOAT_EQ(0.0f, orientation->data.beta);
  EXPECT_FLOAT_EQ(-90.0f, orientation->data.gamma);
}

// Tests a device lying on its right-edge, such that the W3C boundary [-90,90)
// causes the gamma value to become negative.
TEST_F(SensorManagerChromeOSTest, RightEdge) {
  OnAccelerationIncludingGravity(kMeanGravity, 0.0f, 0.0f);

  DeviceMotionHardwareBuffer* motion = motion_buffer();
  EXPECT_FLOAT_EQ(kMeanGravity, motion->data.accelerationIncludingGravityX);
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityY);
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityZ);

  DeviceOrientationHardwareBuffer* orientation = orientation_buffer();
  EXPECT_FLOAT_EQ(0.0f, orientation->data.beta);
  EXPECT_FLOAT_EQ(-90.0f, orientation->data.gamma);
}

// Tests for positive gamma value before the device is completely on its right
// side.
TEST_F(SensorManagerChromeOSTest, BeforeRightEdgeBoundary) {
  OnAccelerationIncludingGravity(kMeanGravity / 2.0f, 0.0f,
                                 -kMeanGravity / 2.0f);

  DeviceMotionHardwareBuffer* motion = motion_buffer();
  EXPECT_FLOAT_EQ(kMeanGravity / 2.0f,
                  motion->data.accelerationIncludingGravityX);
  EXPECT_FLOAT_EQ(0.0f, motion->data.accelerationIncludingGravityY);
  EXPECT_FLOAT_EQ(-kMeanGravity / 2.0f,
                  motion->data.accelerationIncludingGravityZ);

  DeviceOrientationHardwareBuffer* orientation = orientation_buffer();
  EXPECT_FLOAT_EQ(0.0f, orientation->data.beta);
  EXPECT_FLOAT_EQ(45.0f, orientation->data.gamma);
}

}  // namespace content
