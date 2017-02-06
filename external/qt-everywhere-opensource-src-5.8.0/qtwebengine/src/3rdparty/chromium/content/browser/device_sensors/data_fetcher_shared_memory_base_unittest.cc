// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/data_fetcher_shared_memory_base.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/common/device_sensors/device_light_hardware_buffer.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FakeDataFetcher : public DataFetcherSharedMemoryBase {
 public:
  FakeDataFetcher()
      : start_light_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
        start_motion_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        start_orientation_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED),
        start_orientation_absolute_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
        stop_light_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
        stop_motion_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
        stop_orientation_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::NOT_SIGNALED),
        stop_orientation_absolute_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
        updated_light_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED),
        updated_motion_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        updated_orientation_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
        updated_orientation_absolute_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
        light_buffer_(nullptr),
        motion_buffer_(nullptr),
        orientation_buffer_(nullptr),
        orientation_absolute_buffer_(nullptr) {}
  ~FakeDataFetcher() override {}

  bool Init(ConsumerType consumer_type, void* buffer) {
    EXPECT_TRUE(buffer);

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
        break;
      case CONSUMER_TYPE_ORIENTATION:
        orientation_buffer_ =
            static_cast<DeviceOrientationHardwareBuffer*>(buffer);
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        orientation_absolute_buffer_ =
            static_cast<DeviceOrientationHardwareBuffer*>(buffer);
        break;
      case CONSUMER_TYPE_LIGHT:
        light_buffer_ = static_cast<DeviceLightHardwareBuffer*>(buffer);
        break;
      default:
        return false;
    }
    return true;
  }

  void UpdateLight() {
    DeviceLightHardwareBuffer* buffer = GetLightBuffer();
    ASSERT_TRUE(buffer);
    buffer->seqlock.WriteBegin();
    buffer->data.value = 100;
    buffer->seqlock.WriteEnd();
    updated_light_.Signal();
  }

  void UpdateMotion() {
    DeviceMotionHardwareBuffer* buffer = GetMotionBuffer();
    ASSERT_TRUE(buffer);
    buffer->seqlock.WriteBegin();
    buffer->data.interval = kInertialSensorIntervalMicroseconds / 1000.;
    buffer->seqlock.WriteEnd();
    updated_motion_.Signal();
  }

  void UpdateOrientation() {
    DeviceOrientationHardwareBuffer* buffer = GetOrientationBuffer();
    ASSERT_TRUE(buffer);
    buffer->seqlock.WriteBegin();
    buffer->data.alpha = 1;
    buffer->seqlock.WriteEnd();
    updated_orientation_.Signal();
  }

  void UpdateOrientationAbsolute() {
    DeviceOrientationHardwareBuffer* buffer = GetOrientationAbsoluteBuffer();
    ASSERT_TRUE(buffer);
    buffer->seqlock.WriteBegin();
    buffer->data.alpha = 1;
    buffer->data.absolute = true;
    buffer->seqlock.WriteEnd();
    updated_orientation_absolute_.Signal();
  }

  DeviceLightHardwareBuffer* GetLightBuffer() const { return light_buffer_; }

  DeviceMotionHardwareBuffer* GetMotionBuffer() const {
    return motion_buffer_;
  }

  DeviceOrientationHardwareBuffer* GetOrientationBuffer() const {
    return orientation_buffer_;
  }

  DeviceOrientationHardwareBuffer* GetOrientationAbsoluteBuffer() const {
    return orientation_absolute_buffer_;
  }

  void WaitForStart(ConsumerType consumer_type) {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        start_motion_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        start_orientation_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        start_orientation_absolute_.Wait();
        break;
      case CONSUMER_TYPE_LIGHT:
        start_light_.Wait();
        break;
    }
  }

  void WaitForStop(ConsumerType consumer_type) {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stop_motion_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stop_orientation_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        stop_orientation_absolute_.Wait();
        break;
      case CONSUMER_TYPE_LIGHT:
        stop_light_.Wait();
        break;
    }
  }

  void WaitForUpdate(ConsumerType consumer_type) {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        updated_motion_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        updated_orientation_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        updated_orientation_absolute_.Wait();
        break;
      case CONSUMER_TYPE_LIGHT:
        updated_light_.Wait();
        break;
    }
  }

 protected:
  base::WaitableEvent start_light_;
  base::WaitableEvent start_motion_;
  base::WaitableEvent start_orientation_;
  base::WaitableEvent start_orientation_absolute_;
  base::WaitableEvent stop_light_;
  base::WaitableEvent stop_motion_;
  base::WaitableEvent stop_orientation_;
  base::WaitableEvent stop_orientation_absolute_;
  base::WaitableEvent updated_light_;
  base::WaitableEvent updated_motion_;
  base::WaitableEvent updated_orientation_;
  base::WaitableEvent updated_orientation_absolute_;

 private:
  DeviceLightHardwareBuffer* light_buffer_;
  DeviceMotionHardwareBuffer* motion_buffer_;
  DeviceOrientationHardwareBuffer* orientation_buffer_;
  DeviceOrientationHardwareBuffer* orientation_absolute_buffer_;

  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
};

class FakeNonPollingDataFetcher : public FakeDataFetcher {
 public:
  FakeNonPollingDataFetcher() : update_(true) {}
  ~FakeNonPollingDataFetcher() override {}

  bool Start(ConsumerType consumer_type, void* buffer) override {
    Init(consumer_type, buffer);
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        if (update_) UpdateMotion();
        start_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        if (update_) UpdateOrientation();
        start_orientation_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        if (update_) UpdateOrientationAbsolute();
        start_orientation_absolute_.Signal();
        break;
      case CONSUMER_TYPE_LIGHT:
        if (update_) UpdateLight();
        start_light_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  bool Stop(ConsumerType consumer_type) override {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stop_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stop_orientation_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        stop_orientation_absolute_.Signal();
        break;
      case CONSUMER_TYPE_LIGHT:
        stop_light_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  void Fetch(unsigned consumer_bitmask) override {
    FAIL() << "fetch should not be called, "
        << "because this is a non-polling fetcher";
  }

  FetcherType GetType() const override { return FakeDataFetcher::GetType(); }
  void set_update(bool update) { update_ = update; }

 private:
  bool update_;

  DISALLOW_COPY_AND_ASSIGN(FakeNonPollingDataFetcher);
};

class FakePollingDataFetcher : public FakeDataFetcher {
 public:
  FakePollingDataFetcher() { }
  ~FakePollingDataFetcher() override {}

  bool Start(ConsumerType consumer_type, void* buffer) override {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());

    Init(consumer_type, buffer);
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        start_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        start_orientation_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        start_orientation_absolute_.Signal();
        break;
      case CONSUMER_TYPE_LIGHT:
        start_light_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  bool Stop(ConsumerType consumer_type) override {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stop_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stop_orientation_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        stop_orientation_absolute_.Signal();
        break;
      case CONSUMER_TYPE_LIGHT:
        stop_light_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  void Fetch(unsigned consumer_bitmask) override {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());
    EXPECT_TRUE(consumer_bitmask & CONSUMER_TYPE_ORIENTATION ||
                consumer_bitmask & CONSUMER_TYPE_ORIENTATION_ABSOLUTE ||
                consumer_bitmask & CONSUMER_TYPE_MOTION ||
                consumer_bitmask & CONSUMER_TYPE_LIGHT);

    if (consumer_bitmask & CONSUMER_TYPE_ORIENTATION)
      UpdateOrientation();
    if (consumer_bitmask & CONSUMER_TYPE_ORIENTATION_ABSOLUTE)
      UpdateOrientationAbsolute();
    if (consumer_bitmask & CONSUMER_TYPE_MOTION)
      UpdateMotion();
    if (consumer_bitmask & CONSUMER_TYPE_LIGHT)
      UpdateLight();
  }

  FetcherType GetType() const override { return FETCHER_TYPE_POLLING_CALLBACK; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePollingDataFetcher);
};

class FakeZeroDelayPollingDataFetcher : public FakeDataFetcher {
 public:
  FakeZeroDelayPollingDataFetcher() { }
  ~FakeZeroDelayPollingDataFetcher() override {}

  bool Start(ConsumerType consumer_type, void* buffer) override {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());

    Init(consumer_type, buffer);
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        start_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        start_orientation_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        start_orientation_absolute_.Signal();
        break;
      case CONSUMER_TYPE_LIGHT:
        start_light_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  bool Stop(ConsumerType consumer_type) override {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stop_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stop_orientation_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        stop_orientation_absolute_.Signal();
        break;
      case CONSUMER_TYPE_LIGHT:
        stop_light_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  void Fetch(unsigned consumer_bitmask) override {
    FAIL() << "fetch should not be called";
  }

  FetcherType GetType() const override { return FETCHER_TYPE_SEPARATE_THREAD; }

  bool IsPollingTimerRunningForTesting() const {
    return FakeDataFetcher::IsPollingTimerRunningForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeZeroDelayPollingDataFetcher);
};


TEST(DataFetcherSharedMemoryBaseTest, DoesStartMotion) {
  FakeNonPollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_DEFAULT,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_MOTION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_MOTION);

  EXPECT_EQ(kInertialSensorIntervalMicroseconds / 1000.,
            fake_data_fetcher.GetMotionBuffer()->data.interval);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_MOTION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesStartOrientation) {
  FakeNonPollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_DEFAULT,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationBuffer()->data.alpha);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesStartOrientationAbsolute) {
  FakeNonPollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_DEFAULT,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION_ABSOLUTE));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationAbsoluteBuffer()->data.alpha);
  EXPECT_TRUE(fake_data_fetcher.GetOrientationAbsoluteBuffer()->data.absolute);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesStartLight) {
  FakeNonPollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_DEFAULT,
            fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_LIGHT));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_LIGHT);

  EXPECT_EQ(100, fake_data_fetcher.GetLightBuffer()->data.value);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_LIGHT);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_LIGHT);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollMotion) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_POLLING_CALLBACK,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_MOTION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_MOTION);

  EXPECT_EQ(kInertialSensorIntervalMicroseconds / 1000.,
            fake_data_fetcher.GetMotionBuffer()->data.interval);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_MOTION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollOrientation) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_POLLING_CALLBACK,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_ORIENTATION);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationBuffer()->data.alpha);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollOrientationAbsolute) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_POLLING_CALLBACK,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION_ABSOLUTE));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationAbsoluteBuffer()->data.alpha);
  EXPECT_TRUE(fake_data_fetcher.GetOrientationAbsoluteBuffer()->data.absolute);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollLight) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_POLLING_CALLBACK,
            fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_LIGHT));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_LIGHT);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_LIGHT);

  EXPECT_EQ(100, fake_data_fetcher.GetLightBuffer()->data.value);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_LIGHT);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_LIGHT);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollMotionAndOrientation) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_POLLING_CALLBACK,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  base::SharedMemoryHandle handle_orientation =
      fake_data_fetcher.GetSharedMemoryHandleForProcess(
          CONSUMER_TYPE_ORIENTATION, base::GetCurrentProcessHandle());
  EXPECT_TRUE(base::SharedMemory::IsHandleValid(handle_orientation));

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_MOTION));
  base::SharedMemoryHandle handle_motion =
      fake_data_fetcher.GetSharedMemoryHandleForProcess(
          CONSUMER_TYPE_MOTION, base::GetCurrentProcessHandle());
  EXPECT_TRUE(base::SharedMemory::IsHandleValid(handle_motion));

  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_MOTION);

  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_MOTION);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationBuffer()->data.alpha);
  EXPECT_EQ(kInertialSensorIntervalMicroseconds / 1000.,
            fake_data_fetcher.GetMotionBuffer()->data.interval);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_MOTION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesNotPollZeroDelay) {
  FakeZeroDelayPollingDataFetcher fake_data_fetcher;
  EXPECT_EQ(DataFetcherSharedMemoryBase::FETCHER_TYPE_SEPARATE_THREAD,
      fake_data_fetcher.GetType());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);

  EXPECT_FALSE(fake_data_fetcher.IsPollingTimerRunningForTesting());
  EXPECT_EQ(0, fake_data_fetcher.GetOrientationBuffer()->data.alpha);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesClearBufferOnStart) {
  FakeNonPollingDataFetcher fake_data_fetcher;
  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);
  EXPECT_EQ(1, fake_data_fetcher.GetOrientationBuffer()->data.alpha);
  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);

  // Restart orientation without updating the memory buffer and check that
  // it has been cleared to its initial state.
  fake_data_fetcher.set_update(false);
  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);
  EXPECT_EQ(0, fake_data_fetcher.GetOrientationBuffer()->data.alpha);
  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
}

}  // namespace

}  // namespace content
