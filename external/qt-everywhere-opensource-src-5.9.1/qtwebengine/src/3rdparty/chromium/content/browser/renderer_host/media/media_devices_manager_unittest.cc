// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_manager.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace content {

namespace {

// Number of client enumerations to simulate on each test run.
// This allows testing that a single call to low-level enumeration functions
// is performed when cache is enabled, regardless of the number of client calls.
const int kNumCalls = 3;

// This class mocks the audio manager and overrides some methods to ensure that
// we can run simulate device changes.
class MockAudioManager : public media::FakeAudioManager {
 public:
  MockAudioManager()
      : FakeAudioManager(base::ThreadTaskRunnerHandle::Get(),
                         base::ThreadTaskRunnerHandle::Get(),
                         &fake_audio_log_factory_),
        num_output_devices_(2),
        num_input_devices_(2) {}
  ~MockAudioManager() override {}

  MOCK_METHOD1(MockGetAudioInputDeviceNames, void(media::AudioDeviceNames*));
  MOCK_METHOD1(MockGetAudioOutputDeviceNames, void(media::AudioDeviceNames*));

  void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());
    for (size_t i = 0; i < num_input_devices_; i++) {
      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::SizeTToString(i),
          std::string("fake_device_id_") + base::SizeTToString(i)));
    }
    MockGetAudioInputDeviceNames(device_names);
  }

  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());
    for (size_t i = 0; i < num_output_devices_; i++) {
      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::SizeTToString(i),
          std::string("fake_device_id_") + base::SizeTToString(i)));
    }
    MockGetAudioOutputDeviceNames(device_names);
  }

  media::AudioParameters GetDefaultOutputStreamParameters() override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, 48000, 16, 128);
  }

  media::AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, 48000, 16, 128);
  }

  void SetNumAudioOutputDevices(size_t num_devices) {
    num_output_devices_ = num_devices;
  }

  void SetNumAudioInputDevices(size_t num_devices) {
    num_input_devices_ = num_devices;
  }

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
  size_t num_output_devices_;
  size_t num_input_devices_;
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

// This class mocks the video capture device factory and overrides some methods
// to ensure that we can simulate device changes.
class MockVideoCaptureDeviceFactory
    : public media::FakeVideoCaptureDeviceFactory {
 public:
  MockVideoCaptureDeviceFactory() {}
  ~MockVideoCaptureDeviceFactory() override {}

  MOCK_METHOD0(MockGetDeviceDescriptors, void());
  void GetDeviceDescriptors(
      media::VideoCaptureDeviceDescriptors* device_descriptors) override {
    media::FakeVideoCaptureDeviceFactory::GetDeviceDescriptors(
        device_descriptors);
    MockGetDeviceDescriptors();
  }
};

class MockMediaDeviceChangeSubscriber : public MediaDeviceChangeSubscriber {
 public:
  MOCK_METHOD2(OnDevicesChanged,
               void(MediaDeviceType, const MediaDeviceInfoArray&));
};

}  // namespace

class MediaDevicesManagerTest : public ::testing::Test {
 public:
  MediaDevicesManagerTest()
      : video_capture_device_factory_(nullptr),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~MediaDevicesManagerTest() override {}

  MOCK_METHOD1(MockCallback, void(const MediaDeviceEnumeration&));

  void EnumerateCallback(base::RunLoop* run_loop,
                         const MediaDeviceEnumeration& result) {
    MockCallback(result);
    run_loop->Quit();
  }

 protected:
  void SetUp() override {
    audio_manager_.reset(new MockAudioManager());
    video_capture_manager_ = new VideoCaptureManager(
        std::unique_ptr<media::VideoCaptureDeviceFactory>(
            new MockVideoCaptureDeviceFactory()));
    video_capture_manager_->Register(nullptr,
                                     base::ThreadTaskRunnerHandle::Get());
    video_capture_device_factory_ = static_cast<MockVideoCaptureDeviceFactory*>(
        video_capture_manager_->video_capture_device_factory());
    media_devices_manager_.reset(new MediaDevicesManager(
        audio_manager_.get(), video_capture_manager_, nullptr));
  }

  void EnableCache(MediaDeviceType type) {
    media_devices_manager_->SetCachePolicy(
        type, MediaDevicesManager::CachePolicy::SYSTEM_MONITOR);
  }

  std::unique_ptr<MediaDevicesManager> media_devices_manager_;
  scoped_refptr<VideoCaptureManager> video_capture_manager_;
  MockVideoCaptureDeviceFactory* video_capture_device_factory_;
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<MockAudioManager, media::AudioManagerDeleter> audio_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDevicesManagerTest);
};

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheAudioInput) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheVideoInput) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(kNumCalls);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheAudioOutput) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheAudio) {
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheAudio) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(1);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(1);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EnableCache(MEDIA_DEVICE_TYPE_AUDIO_INPUT);
  EnableCache(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheVideo) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(1);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EnableCache(MEDIA_DEVICE_TYPE_VIDEO_INPUT);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheAudioWithDeviceChanges) {
  MediaDeviceEnumeration enumeration;
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(2);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(2);
  EXPECT_CALL(*this, MockCallback(_))
      .Times(2 * kNumCalls)
      .WillRepeatedly(SaveArg<0>(&enumeration));

  size_t num_audio_input_devices = 5;
  size_t num_audio_output_devices = 3;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  EnableCache(MEDIA_DEVICE_TYPE_AUDIO_INPUT);
  EnableCache(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(num_audio_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT].size());
    EXPECT_EQ(num_audio_output_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size());
  }

  // Simulate device change
  num_audio_input_devices = 3;
  num_audio_output_devices = 4;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  media_devices_manager_->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);

  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(num_audio_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT].size());
    EXPECT_EQ(num_audio_output_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size());
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheVideoWithDeviceChanges) {
  MediaDeviceEnumeration enumeration;
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(2);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_))
      .Times(2 * kNumCalls)
      .WillRepeatedly(SaveArg<0>(&enumeration));

  // Simulate device change
  size_t num_video_input_devices = 5;
  video_capture_device_factory_->set_number_of_devices(num_video_input_devices);
  EnableCache(MEDIA_DEVICE_TYPE_VIDEO_INPUT);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(num_video_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT].size());
  }

  // Simulate device change
  num_video_input_devices = 9;
  video_capture_device_factory_->set_number_of_devices(num_video_input_devices);
  media_devices_manager_->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(num_video_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT].size());
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheAllWithDeviceChanges) {
  MediaDeviceEnumeration enumeration;
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(2);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(2);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(2);
  EXPECT_CALL(*this, MockCallback(_))
      .Times(2 * kNumCalls)
      .WillRepeatedly(SaveArg<0>(&enumeration));

  size_t num_audio_input_devices = 5;
  size_t num_video_input_devices = 4;
  size_t num_audio_output_devices = 3;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->set_number_of_devices(num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  EnableCache(MEDIA_DEVICE_TYPE_AUDIO_INPUT);
  EnableCache(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT);
  EnableCache(MEDIA_DEVICE_TYPE_VIDEO_INPUT);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(num_audio_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT].size());
    EXPECT_EQ(num_video_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT].size());
    EXPECT_EQ(num_audio_output_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size());
  }

  // Simulate device changes
  num_audio_input_devices = 3;
  num_video_input_devices = 2;
  num_audio_output_devices = 4;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->set_number_of_devices(num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  media_devices_manager_->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  media_devices_manager_->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                   base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(num_audio_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT].size());
    EXPECT_EQ(num_video_input_devices,
              enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT].size());
    EXPECT_EQ(num_audio_output_devices,
              enumeration[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size());
  }
}

TEST_F(MediaDevicesManagerTest, SubscribeDeviceChanges) {
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(3);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDeviceDescriptors())
      .Times(3);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(3);

  size_t num_audio_input_devices = 5;
  size_t num_video_input_devices = 4;
  size_t num_audio_output_devices = 3;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->set_number_of_devices(num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);

  // Run an enumeration to make sure |media_devices_manager_| has the new
  // configuration.
  EXPECT_CALL(*this, MockCallback(_));
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  base::RunLoop run_loop;
  media_devices_manager_->EnumerateDevices(
      devices_to_enumerate,
      base::Bind(&MediaDevicesManagerTest::EnumerateCallback,
                 base::Unretained(this), &run_loop));
  run_loop.Run();

  // Add device-change event subscribers.
  MockMediaDeviceChangeSubscriber subscriber_audio_input;
  MockMediaDeviceChangeSubscriber subscriber_video_input;
  MockMediaDeviceChangeSubscriber subscriber_audio_output;
  MockMediaDeviceChangeSubscriber subscriber_all;

  media_devices_manager_->SubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_AUDIO_INPUT, &subscriber_audio_input);
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_VIDEO_INPUT, &subscriber_video_input);
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, &subscriber_audio_output);
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_AUDIO_INPUT, &subscriber_all);
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_VIDEO_INPUT, &subscriber_all);
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, &subscriber_all);

  MediaDeviceInfoArray notification_audio_input;
  MediaDeviceInfoArray notification_video_input;
  MediaDeviceInfoArray notification_audio_output;
  MediaDeviceInfoArray notification_all_audio_input;
  MediaDeviceInfoArray notification_all_video_input;
  MediaDeviceInfoArray notification_all_audio_output;
  EXPECT_CALL(subscriber_audio_input,
              OnDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_INPUT, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&notification_audio_input));
  EXPECT_CALL(subscriber_video_input,
              OnDevicesChanged(MEDIA_DEVICE_TYPE_VIDEO_INPUT, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&notification_video_input));
  EXPECT_CALL(subscriber_audio_output,
              OnDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&notification_audio_output));
  EXPECT_CALL(subscriber_all,
              OnDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_INPUT, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&notification_all_audio_input));
  EXPECT_CALL(subscriber_all,
              OnDevicesChanged(MEDIA_DEVICE_TYPE_VIDEO_INPUT, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&notification_all_video_input));
  EXPECT_CALL(subscriber_all,
              OnDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&notification_all_audio_output));

  // Simulate device changes.
  num_audio_input_devices = 3;
  num_video_input_devices = 2;
  num_audio_output_devices = 4;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->set_number_of_devices(num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  media_devices_manager_->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  media_devices_manager_->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(num_audio_input_devices, notification_audio_input.size());
  EXPECT_EQ(num_video_input_devices, notification_video_input.size());
  EXPECT_EQ(num_audio_output_devices, notification_audio_output.size());
  EXPECT_EQ(num_audio_input_devices, notification_all_audio_input.size());
  EXPECT_EQ(num_video_input_devices, notification_all_video_input.size());
  EXPECT_EQ(num_audio_output_devices, notification_all_audio_output.size());

  media_devices_manager_->UnsubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_AUDIO_INPUT, &subscriber_audio_input);
  media_devices_manager_->UnsubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_VIDEO_INPUT, &subscriber_video_input);
  media_devices_manager_->UnsubscribeDeviceChangeNotifications(
      MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, &subscriber_audio_output);

  // Simulate further device changes. Only the objects still subscribed to the
  // device-change events will receive notifications.
  num_audio_input_devices = 2;
  num_video_input_devices = 1;
  num_audio_output_devices = 3;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->set_number_of_devices(num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  media_devices_manager_->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  media_devices_manager_->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(num_audio_input_devices, notification_all_audio_input.size());
  EXPECT_EQ(num_video_input_devices, notification_all_video_input.size());
  EXPECT_EQ(num_audio_output_devices, notification_all_audio_output.size());
}

}  // namespace content
