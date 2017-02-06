// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_orientation_event_pump.h"

#include <string.h>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"

namespace content {

class MockDeviceOrientationListener
    : public blink::WebDeviceOrientationListener {
 public:
  MockDeviceOrientationListener() : did_change_device_orientation_(false) {
    memset(&data_, 0, sizeof(data_));
  }
  ~MockDeviceOrientationListener() override {}

  void didChangeDeviceOrientation(
      const blink::WebDeviceOrientationData& data) override {
    memcpy(&data_, &data, sizeof(data));
    did_change_device_orientation_ = true;
  }

  bool did_change_device_orientation() const {
    return did_change_device_orientation_;
  }
  void set_did_change_device_orientation(bool value) {
    did_change_device_orientation_ = value;
  }
  const blink::WebDeviceOrientationData& data() const {
    return data_;
  }

 private:
  bool did_change_device_orientation_;
  blink::WebDeviceOrientationData data_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceOrientationListener);
};

class DeviceOrientationEventPumpForTesting : public DeviceOrientationEventPump {
 public:
  DeviceOrientationEventPumpForTesting()
      : DeviceOrientationEventPump(0) { }
  ~DeviceOrientationEventPumpForTesting() override {}

  void OnDidStart(base::SharedMemoryHandle renderer_handle) {
    DeviceOrientationEventPump::OnDidStart(renderer_handle);
  }
  void SendStartMessage() override {}
  void SendStopMessage() override {}
  void FireEvent() override {
    DeviceOrientationEventPump::FireEvent();
    Stop();
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpForTesting);
};

class DeviceOrientationEventPumpTest : public testing::Test {
 public:
  DeviceOrientationEventPumpTest() {
      EXPECT_TRUE(shared_memory_.CreateAndMapAnonymous(
          sizeof(DeviceOrientationHardwareBuffer)));
  }

 protected:
  void SetUp() override {
    const DeviceOrientationHardwareBuffer* null_buffer = nullptr;
    listener_.reset(new MockDeviceOrientationListener);
    orientation_pump_.reset(new DeviceOrientationEventPumpForTesting);
    buffer_ = static_cast<DeviceOrientationHardwareBuffer*>(
        shared_memory_.memory());
    ASSERT_NE(null_buffer, buffer_);
    memset(buffer_, 0, sizeof(DeviceOrientationHardwareBuffer));
    ASSERT_TRUE(shared_memory_.ShareToProcess(base::GetCurrentProcessHandle(),
        &handle_));
  }

  void InitBuffer() {
    blink::WebDeviceOrientationData& data = buffer_->data;
    data.alpha = 1;
    data.hasAlpha = true;
    data.beta = 2;
    data.hasBeta = true;
    data.gamma = 3;
    data.hasGamma = true;
    data.allAvailableSensorsAreActive = true;
  }

  void InitBufferNoData() {
    blink::WebDeviceOrientationData& data = buffer_->data;
    data.allAvailableSensorsAreActive = true;
  }

  MockDeviceOrientationListener* listener() { return listener_.get(); }
  DeviceOrientationEventPumpForTesting* orientation_pump() {
    return orientation_pump_.get();
  }
  base::SharedMemoryHandle handle() { return handle_; }
  DeviceOrientationHardwareBuffer* buffer() { return buffer_; }

 private:
  std::unique_ptr<MockDeviceOrientationListener> listener_;
  std::unique_ptr<DeviceOrientationEventPumpForTesting> orientation_pump_;
  base::SharedMemoryHandle handle_;
  base::SharedMemory shared_memory_;
  DeviceOrientationHardwareBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpTest);
};

TEST_F(DeviceOrientationEventPumpTest, DidStartPolling) {
  base::MessageLoop loop;

  InitBuffer();
  orientation_pump()->Start(listener());
  orientation_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const blink::WebDeviceOrientationData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.allAvailableSensorsAreActive);
  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.hasAlpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.hasBeta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.hasGamma);
}

TEST_F(DeviceOrientationEventPumpTest, FireAllNullEvent) {
  base::MessageLoop loop;

  InitBufferNoData();
  orientation_pump()->Start(listener());
  orientation_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const blink::WebDeviceOrientationData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.allAvailableSensorsAreActive);
  EXPECT_FALSE(received_data.hasAlpha);
  EXPECT_FALSE(received_data.hasBeta);
  EXPECT_FALSE(received_data.hasGamma);
}

TEST_F(DeviceOrientationEventPumpTest, UpdateRespectsOrientationThreshold) {
  base::MessageLoop loop;

  InitBuffer();
  orientation_pump()->Start(listener());
  orientation_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const blink::WebDeviceOrientationData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.allAvailableSensorsAreActive);
  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.hasAlpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.hasBeta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.hasGamma);

  buffer()->data.alpha =
      1 + DeviceOrientationEventPump::kOrientationThreshold / 2.0;
  listener()->set_did_change_device_orientation(false);

  // Reset the pump's listener.
  orientation_pump()->Start(listener());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DeviceOrientationEventPumpForTesting::FireEvent,
                            base::Unretained(orientation_pump())));
  base::MessageLoop::current()->Run();

  EXPECT_FALSE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.allAvailableSensorsAreActive);
  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.hasAlpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.hasBeta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.hasGamma);

  buffer()->data.alpha =
      1 + DeviceOrientationEventPump::kOrientationThreshold;
  listener()->set_did_change_device_orientation(false);

  // Reset the pump's listener.
  orientation_pump()->Start(listener());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DeviceOrientationEventPumpForTesting::FireEvent,
                            base::Unretained(orientation_pump())));
  base::MessageLoop::current()->Run();

  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_EQ(1 + DeviceOrientationEventPump::kOrientationThreshold,
      static_cast<double>(received_data.alpha));
}

}  // namespace content
