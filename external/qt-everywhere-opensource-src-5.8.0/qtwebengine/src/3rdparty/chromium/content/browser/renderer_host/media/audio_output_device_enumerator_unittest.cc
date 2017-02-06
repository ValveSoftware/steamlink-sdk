// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_device_enumerator.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

namespace {

class MockAudioManager : public media::FakeAudioManager {
 public:
  MockAudioManager(size_t num_devices)
      : FakeAudioManager(base::ThreadTaskRunnerHandle::Get(),
                         base::ThreadTaskRunnerHandle::Get(),
                         &fake_audio_log_factory_),
        num_devices_(num_devices) {}
  MockAudioManager() : MockAudioManager(0) {}
  ~MockAudioManager() override {}

  MOCK_METHOD1(MockGetAudioOutputDeviceNames, void(media::AudioDeviceNames*));

  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());
    MockGetAudioOutputDeviceNames(device_names);
    if (num_devices_ > 0) {
      device_names->push_back(media::AudioDeviceName::CreateDefault());
      for (size_t i = 0; i < num_devices_; i++) {
        device_names->push_back(
            media::AudioDeviceName("FakeDeviceName_" + base::IntToString(i),
                                   "FakeDeviceId_" + base::IntToString(i)));
      }
    }
  }

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
  size_t num_devices_;
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

// Fake audio manager that exposes only the default device
class OnlyDefaultDeviceAudioManager : public MockAudioManager {
 public:
  OnlyDefaultDeviceAudioManager() {}
  ~OnlyDefaultDeviceAudioManager() override {}
  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());
    MockGetAudioOutputDeviceNames(device_names);
    device_names->push_front(media::AudioDeviceName::CreateDefault());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OnlyDefaultDeviceAudioManager);
};

}  // namespace

class AudioOutputDeviceEnumeratorTest : public ::testing::Test {
 public:
  AudioOutputDeviceEnumeratorTest() {}
  ~AudioOutputDeviceEnumeratorTest() override {}

  MOCK_METHOD1(MockCallback, void(const AudioOutputDeviceEnumeration&));

  // Performs n calls to MockCallback (one by QuitCallback)
  // and n-1 calls to Enumerate
  void EnumerateCallback(AudioOutputDeviceEnumerator* enumerator,
                         int n,
                         const AudioOutputDeviceEnumeration& result) {
    MockCallback(result);
    if (n > 1) {
      enumerator->Enumerate(
          base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCallback,
                     base::Unretained(this), enumerator, n - 1));
    } else {
      enumerator->Enumerate(
          base::Bind(&AudioOutputDeviceEnumeratorTest::QuitCallback,
                     base::Unretained(this)));
    }
  }

  void EnumerateCountCallback(size_t num_entries_expected,
                              bool actual_devices_expected,
                              const AudioOutputDeviceEnumeration& result) {
    EXPECT_EQ(actual_devices_expected, result.has_actual_devices);
    EXPECT_EQ(num_entries_expected, result.devices.size());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

  void QuitCallback(const AudioOutputDeviceEnumeration& result) {
    MockCallback(result);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<MockAudioManager, media::AudioManagerDeleter> audio_manager_;
  base::RunLoop run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceEnumeratorTest);
};

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateWithCache) {
  const int num_calls = 10;
  audio_manager_.reset(new MockAudioManager());
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(1);
  EXPECT_CALL(*this, MockCallback(_)).Times(num_calls);
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_MANUAL_INVALIDATION);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCallback,
                 base::Unretained(this), &enumerator, num_calls - 1));
  run_loop_.Run();
}

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateWithNoCache) {
  const int num_calls = 10;
  audio_manager_.reset(new MockAudioManager());
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_))
      .Times(num_calls);
  EXPECT_CALL(*this, MockCallback(_)).Times(num_calls);
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCallback,
                 base::Unretained(this), &enumerator, num_calls - 1));
  run_loop_.Run();
}

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateNoDevices) {
  audio_manager_.reset(new MockAudioManager());
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_));
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCountCallback,
                 base::Unretained(this), 1, false));
  run_loop_.Run();
}

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateOnlyDefaultDevice) {
  audio_manager_.reset(new OnlyDefaultDeviceAudioManager());
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_));
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCountCallback,
                 base::Unretained(this), 1, true));
  run_loop_.Run();
}

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateOneDevice) {
  size_t num_devices = 1;
  audio_manager_.reset(new MockAudioManager(num_devices));
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_));
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCountCallback,
                 base::Unretained(this), num_devices + 1, true));
  run_loop_.Run();
}

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateMultipleDevices) {
  size_t num_devices = 5;
  audio_manager_.reset(new MockAudioManager(num_devices));
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_));
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCountCallback,
                 base::Unretained(this), num_devices + 1, true));
  run_loop_.Run();
}

}  // namespace content
