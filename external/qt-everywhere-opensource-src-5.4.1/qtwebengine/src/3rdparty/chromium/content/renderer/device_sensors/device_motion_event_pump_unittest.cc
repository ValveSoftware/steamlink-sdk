// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_motion_event_pump.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionListener.h"

namespace content {

class MockDeviceMotionListener : public blink::WebDeviceMotionListener {
 public:
  MockDeviceMotionListener() : did_change_device_motion_(false) {
    memset(&data_, 0, sizeof(data_));
  }
  virtual ~MockDeviceMotionListener() { }

  virtual void didChangeDeviceMotion(
      const blink::WebDeviceMotionData& data) OVERRIDE {
    memcpy(&data_, &data, sizeof(data));
    did_change_device_motion_ = true;
  }

  bool did_change_device_motion() const {
    return did_change_device_motion_;
  }
  const blink::WebDeviceMotionData& data() const {
    return data_;
  }

 private:
  bool did_change_device_motion_;
  blink::WebDeviceMotionData data_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceMotionListener);
};

class DeviceMotionEventPumpForTesting : public DeviceMotionEventPump {
 public:
  DeviceMotionEventPumpForTesting() { }
  virtual ~DeviceMotionEventPumpForTesting() { }

  void OnDidStart(base::SharedMemoryHandle renderer_handle) {
    DeviceMotionEventPump::OnDidStart(renderer_handle);
  }
  virtual bool SendStartMessage() OVERRIDE { return true; }
  virtual bool SendStopMessage() OVERRIDE { return true; }
  virtual void FireEvent() OVERRIDE {
    DeviceMotionEventPump::FireEvent();
    Stop();
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPumpForTesting);
};

class DeviceMotionEventPumpTest : public testing::Test {
 public:
  DeviceMotionEventPumpTest() {
    EXPECT_TRUE(shared_memory_.CreateAndMapAnonymous(
        sizeof(DeviceMotionHardwareBuffer)));
  }

 protected:
  virtual void SetUp() OVERRIDE {
    const DeviceMotionHardwareBuffer* null_buffer = NULL;
    listener_.reset(new MockDeviceMotionListener);
    motion_pump_.reset(new DeviceMotionEventPumpForTesting);
    buffer_ = static_cast<DeviceMotionHardwareBuffer*>(shared_memory_.memory());
    ASSERT_NE(null_buffer, buffer_);
    memset(buffer_, 0, sizeof(DeviceMotionHardwareBuffer));
    ASSERT_TRUE(shared_memory_.ShareToProcess(base::GetCurrentProcessHandle(),
        &handle_));
  }

  void InitBuffer(bool allAvailableSensorsActive) {
    blink::WebDeviceMotionData& data = buffer_->data;
    data.accelerationX = 1;
    data.hasAccelerationX = true;
    data.accelerationY = 2;
    data.hasAccelerationY = true;
    data.accelerationZ = 3;
    data.hasAccelerationZ = true;
    data.allAvailableSensorsAreActive = allAvailableSensorsActive;
  }

  MockDeviceMotionListener* listener() { return listener_.get(); }
  DeviceMotionEventPumpForTesting* motion_pump() { return motion_pump_.get(); }
  base::SharedMemoryHandle handle() { return handle_; }

 private:
  scoped_ptr<MockDeviceMotionListener> listener_;
  scoped_ptr<DeviceMotionEventPumpForTesting> motion_pump_;
  base::SharedMemoryHandle handle_;
  base::SharedMemory shared_memory_;
  DeviceMotionHardwareBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPumpTest);
};

TEST_F(DeviceMotionEventPumpTest, DidStartPolling) {
  base::MessageLoopForUI loop;

  InitBuffer(true);

  motion_pump()->SetListener(listener());
  motion_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const blink::WebDeviceMotionData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_motion());
  EXPECT_TRUE(received_data.hasAccelerationX);
  EXPECT_EQ(1, static_cast<double>(received_data.accelerationX));
  EXPECT_TRUE(received_data.hasAccelerationX);
  EXPECT_EQ(2, static_cast<double>(received_data.accelerationY));
  EXPECT_TRUE(received_data.hasAccelerationY);
  EXPECT_EQ(3, static_cast<double>(received_data.accelerationZ));
  EXPECT_TRUE(received_data.hasAccelerationZ);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityX);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityY);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityZ);
  EXPECT_FALSE(received_data.hasRotationRateAlpha);
  EXPECT_FALSE(received_data.hasRotationRateBeta);
  EXPECT_FALSE(received_data.hasRotationRateGamma);
}

TEST_F(DeviceMotionEventPumpTest, DidStartPollingNotAllSensorsActive) {
  base::MessageLoopForUI loop;

  InitBuffer(false);

  motion_pump()->SetListener(listener());
  motion_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const blink::WebDeviceMotionData& received_data = listener()->data();
  // No change in device motion because allAvailableSensorsAreActive is false.
  EXPECT_FALSE(listener()->did_change_device_motion());
  EXPECT_FALSE(received_data.hasAccelerationX);
  EXPECT_FALSE(received_data.hasAccelerationX);
  EXPECT_FALSE(received_data.hasAccelerationY);
  EXPECT_FALSE(received_data.hasAccelerationZ);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityX);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityY);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityZ);
  EXPECT_FALSE(received_data.hasRotationRateAlpha);
  EXPECT_FALSE(received_data.hasRotationRateBeta);
  EXPECT_FALSE(received_data.hasRotationRateGamma);
}

}  // namespace content
