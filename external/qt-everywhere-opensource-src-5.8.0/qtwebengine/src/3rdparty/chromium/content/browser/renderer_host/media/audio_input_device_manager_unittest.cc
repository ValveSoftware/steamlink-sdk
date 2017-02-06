// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_manager_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::SaveArg;
using testing::Return;

namespace content {

class MockAudioInputDeviceManagerListener
    : public MediaStreamProviderListener {
 public:
  MockAudioInputDeviceManagerListener() {}
  virtual ~MockAudioInputDeviceManagerListener() {}

  MOCK_METHOD2(Opened, void(MediaStreamType, const int));
  MOCK_METHOD2(Closed, void(MediaStreamType, const int));
  MOCK_METHOD2(DevicesEnumerated, void(MediaStreamType,
                                       const StreamDeviceInfoArray&));
  MOCK_METHOD2(Aborted, void(MediaStreamType, int));

  StreamDeviceInfoArray devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputDeviceManagerListener);
};

// TODO(henrika): there are special restrictions for Android since
// AudioInputDeviceManager::Open() must be called on the audio thread.
// This test suite must be modified to run on Android.
#if defined(OS_ANDROID)
#define MAYBE_AudioInputDeviceManagerTest DISABLED_AudioInputDeviceManagerTest
#else
#define MAYBE_AudioInputDeviceManagerTest AudioInputDeviceManagerTest
#endif

class MAYBE_AudioInputDeviceManagerTest : public testing::Test {
 public:
  MAYBE_AudioInputDeviceManagerTest() {}

 protected:
  void SetUp() override {
    audio_manager_ = media::AudioManager::CreateForTesting(
        base::ThreadTaskRunnerHandle::Get());
    // Flush the message loop to ensure proper initialization of AudioManager.
    base::RunLoop().RunUntilIdle();

    manager_ = new AudioInputDeviceManager(audio_manager_.get());
    manager_->UseFakeDevice();
    audio_input_listener_.reset(new MockAudioInputDeviceManagerListener());
    manager_->Register(audio_input_listener_.get(),
                       audio_manager_->GetTaskRunner());

    // Gets the enumerated device list from the AudioInputDeviceManager.
    manager_->EnumerateDevices(MEDIA_DEVICE_AUDIO_CAPTURE);
    EXPECT_CALL(*audio_input_listener_,
                DevicesEnumerated(MEDIA_DEVICE_AUDIO_CAPTURE, _))
        .Times(1)
        .WillOnce(SaveArg<1>(&devices_));

    // Wait until we get the list.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    manager_->Unregister();
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<AudioInputDeviceManager> manager_;
  std::unique_ptr<MockAudioInputDeviceManagerListener> audio_input_listener_;
  media::ScopedAudioManagerPtr audio_manager_;
  StreamDeviceInfoArray devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_AudioInputDeviceManagerTest);
};

// Opens and closes the devices.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenAndCloseDevice) {

  ASSERT_FALSE(devices_.empty());

  InSequence s;

  for (StreamDeviceInfoArray::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    // Opens/closes the devices.
    int session_id = manager_->Open(*iter);

    // Expected mock call with expected return value.
    EXPECT_CALL(*audio_input_listener_,
                Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
        .Times(1);
    // Waits for the callback.
    base::RunLoop().RunUntilIdle();

    manager_->Close(session_id);
    EXPECT_CALL(*audio_input_listener_,
                Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
        .Times(1);

    // Waits for the callback.
    base::RunLoop().RunUntilIdle();
  }
}

// Opens multiple devices at one time and closes them later.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenMultipleDevices) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  int index = 0;
  std::unique_ptr<int[]> session_id(new int[devices_.size()]);

  // Opens the devices in a loop.
  for (StreamDeviceInfoArray::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter, ++index) {
    // Opens the devices.
    session_id[index] = manager_->Open(*iter);

    // Expected mock call with expected returned value.
    EXPECT_CALL(*audio_input_listener_,
                Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[index]))
        .Times(1);

    // Waits for the callback.
    base::RunLoop().RunUntilIdle();
  }

  // Checks if the session_ids are unique.
  for (size_t i = 0; i < devices_.size() - 1; ++i) {
    for (size_t k = i + 1; k < devices_.size(); ++k) {
      EXPECT_TRUE(session_id[i] != session_id[k]);
    }
  }

  for (size_t i = 0; i < devices_.size(); ++i) {
    // Closes the devices.
    manager_->Close(session_id[i]);
    EXPECT_CALL(*audio_input_listener_,
                Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[i]))
        .Times(1);

    // Waits for the callback.
    base::RunLoop().RunUntilIdle();
  }
}

// Opens a non-existing device.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenNotExistingDevice) {
  InSequence s;

  MediaStreamType stream_type = MEDIA_DEVICE_AUDIO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  int sample_rate(0);
  int channel_config(0);
  StreamDeviceInfo dummy_device(
      stream_type, device_name, device_id, sample_rate, channel_config, 2048);

  int session_id = manager_->Open(dummy_device);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);

  // Waits for the callback.
  base::RunLoop().RunUntilIdle();
}

// Opens default device twice.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenDeviceTwice) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  // Opens and closes the default device twice.
  int first_session_id = manager_->Open(devices_.front());
  int second_session_id = manager_->Open(devices_.front());

  // Expected mock calls with expected returned values.
  EXPECT_NE(first_session_id, second_session_id);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, second_session_id))
      .Times(1);
  // Waits for the callback.
  base::RunLoop().RunUntilIdle();

  manager_->Close(first_session_id);
  manager_->Close(second_session_id);
  EXPECT_CALL(*audio_input_listener_,
              Closed(MEDIA_DEVICE_AUDIO_CAPTURE, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_,
              Closed(MEDIA_DEVICE_AUDIO_CAPTURE, second_session_id))
      .Times(1);
  // Waits for the callback.
  base::RunLoop().RunUntilIdle();
}

// Accesses then closes the sessions after opening the devices.
TEST_F(MAYBE_AudioInputDeviceManagerTest, AccessAndCloseSession) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  int index = 0;
  std::unique_ptr<int[]> session_id(new int[devices_.size()]);

  // Loops through the devices and calls Open()/Close()/GetOpenedDeviceInfoById
  // for each device.
  for (StreamDeviceInfoArray::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter, ++index) {
    // Note that no DeviceStopped() notification for Event Handler as we have
    // stopped the device before calling close.
    session_id[index] = manager_->Open(*iter);
    EXPECT_CALL(*audio_input_listener_,
                Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[index]))
        .Times(1);
    base::RunLoop().RunUntilIdle();

    const StreamDeviceInfo* info = manager_->GetOpenedDeviceInfoById(
        session_id[index]);
    DCHECK(info);
    EXPECT_EQ(iter->device.id, info->device.id);
    manager_->Close(session_id[index]);
    EXPECT_CALL(*audio_input_listener_,
                Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id[index]))
        .Times(1);
    base::RunLoop().RunUntilIdle();
  }
}

// Access an invalid session.
TEST_F(MAYBE_AudioInputDeviceManagerTest, AccessInvalidSession) {
  InSequence s;

  // Opens the first device.
  StreamDeviceInfoArray::const_iterator iter = devices_.begin();
  int session_id = manager_->Open(*iter);
  EXPECT_CALL(*audio_input_listener_,
              Opened(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);
  base::RunLoop().RunUntilIdle();

  // Access a non-opened device.
  // This should fail and return an empty StreamDeviceInfo.
  int invalid_session_id = session_id + 1;
  const StreamDeviceInfo* info =
      manager_->GetOpenedDeviceInfoById(invalid_session_id);
  DCHECK(!info);

  manager_->Close(session_id);
  EXPECT_CALL(*audio_input_listener_,
              Closed(MEDIA_DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
