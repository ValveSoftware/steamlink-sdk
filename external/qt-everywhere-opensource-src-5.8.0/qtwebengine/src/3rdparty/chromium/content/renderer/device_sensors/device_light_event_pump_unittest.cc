// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_light_event_pump.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/device_sensors/device_light_hardware_buffer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebDeviceLightListener.h"

namespace content {

class MockDeviceLightListener : public blink::WebDeviceLightListener {
 public:
  MockDeviceLightListener() : did_change_device_light_(false) {}
  ~MockDeviceLightListener() override {}

  void didChangeDeviceLight(double value) override {
    data_.value = value;
    did_change_device_light_ = true;
  }

  bool did_change_device_light() const { return did_change_device_light_; }

  void set_did_change_device_light(bool value) {
    did_change_device_light_ = value;
  }

  const DeviceLightData& data() const { return data_; }

 private:
  bool did_change_device_light_;
  DeviceLightData data_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceLightListener);
};

class DeviceLightEventPumpForTesting : public DeviceLightEventPump {
 public:
  DeviceLightEventPumpForTesting()
      : DeviceLightEventPump(0) {}
  ~DeviceLightEventPumpForTesting() override {}

  void OnDidStart(base::SharedMemoryHandle renderer_handle) {
    DeviceLightEventPump::OnDidStart(renderer_handle);
  }
  void SendStartMessage() override {}
  void SendStopMessage() override {}
  void FireEvent() override {
    DeviceLightEventPump::FireEvent();
    Stop();
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLightEventPumpForTesting);
};

class DeviceLightEventPumpTest : public testing::Test {
 public:
  DeviceLightEventPumpTest() {
    EXPECT_TRUE(shared_memory_.CreateAndMapAnonymous(
        sizeof(DeviceLightHardwareBuffer)));
  }

 protected:
  void SetUp() override {
    const DeviceLightHardwareBuffer* null_buffer = nullptr;
    listener_.reset(new MockDeviceLightListener);
    light_pump_.reset(new DeviceLightEventPumpForTesting);
    buffer_ = static_cast<DeviceLightHardwareBuffer*>(shared_memory_.memory());
    ASSERT_NE(null_buffer, buffer_);
    ASSERT_TRUE(shared_memory_.ShareToProcess(base::GetCurrentProcessHandle(),
                                              &handle_));
  }

  void InitBuffer() {
    DeviceLightData& data = buffer_->data;
    data.value = 1.0;
  }

  MockDeviceLightListener* listener() { return listener_.get(); }
  DeviceLightEventPumpForTesting* light_pump() { return light_pump_.get(); }
  base::SharedMemoryHandle handle() { return handle_; }
  DeviceLightHardwareBuffer* buffer() { return buffer_; }

 private:
  std::unique_ptr<MockDeviceLightListener> listener_;
  std::unique_ptr<DeviceLightEventPumpForTesting> light_pump_;
  base::SharedMemoryHandle handle_;
  base::SharedMemory shared_memory_;
  DeviceLightHardwareBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLightEventPumpTest);
};

TEST_F(DeviceLightEventPumpTest, DidStartPolling) {
  base::MessageLoopForUI loop;

  InitBuffer();

  light_pump()->Start(listener());
  light_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const DeviceLightData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_light());
  EXPECT_EQ(1, static_cast<double>(received_data.value));
}

TEST_F(DeviceLightEventPumpTest, FireAllNullEvent) {
  base::MessageLoopForUI loop;

  light_pump()->Start(listener());
  light_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const DeviceLightData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_light());
  EXPECT_FALSE(received_data.value);
}

TEST_F(DeviceLightEventPumpTest, DidStartPollingValuesEqual) {
  base::MessageLoopForUI loop;

  InitBuffer();

  light_pump()->Start(listener());
  light_pump()->OnDidStart(handle());

  base::MessageLoop::current()->Run();

  const DeviceLightData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_light());
  EXPECT_EQ(1, static_cast<double>(received_data.value));

  double last_seen_data = received_data.value;
  // Set next value to be same as previous value.
  buffer()->data.value = 1.0;
  listener()->set_did_change_device_light(false);

  // Reset the pump's listener.
  light_pump()->Start(listener());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DeviceLightEventPumpForTesting::FireEvent,
                            base::Unretained(light_pump())));
  base::MessageLoop::current()->Run();

  // No change in device light as present value is same as previous value.
  EXPECT_FALSE(listener()->did_change_device_light());
  EXPECT_EQ(last_seen_data, static_cast<double>(received_data.value));
}

}  // namespace content
