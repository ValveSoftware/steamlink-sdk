// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreAudio/AudioHardware.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "media/audio/mac/audio_device_listener_mac.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioDeviceListenerMacTest : public testing::Test {
 public:
  AudioDeviceListenerMacTest() {
    // It's important to create the device listener from the message loop in
    // order to ensure we don't end up with unbalanced TaskObserver calls.
    message_loop_.PostTask(FROM_HERE, base::Bind(
        &AudioDeviceListenerMacTest::CreateDeviceListener,
        base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  virtual ~AudioDeviceListenerMacTest() {
    // It's important to destroy the device listener from the message loop in
    // order to ensure we don't end up with unbalanced TaskObserver calls.
    message_loop_.PostTask(FROM_HERE, base::Bind(
        &AudioDeviceListenerMacTest::DestroyDeviceListener,
        base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void CreateDeviceListener() {
    // Force a post task using BindToCurrentLoop() to ensure device listener
    // internals are working correctly.
    output_device_listener_.reset(new AudioDeviceListenerMac(BindToCurrentLoop(
        base::Bind(&AudioDeviceListenerMacTest::OnDeviceChange,
                   base::Unretained(this)))));
  }

  void DestroyDeviceListener() {
    output_device_listener_.reset();
  }

  bool ListenerIsValid() {
    return !output_device_listener_->listener_cb_.is_null();
  }

  // Simulate a device change where no output devices are available.
  bool SimulateDefaultOutputDeviceChange() {
    // Include multiple addresses to ensure only a single device change event
    // occurs.
    const AudioObjectPropertyAddress addresses[] = {
      AudioDeviceListenerMac::kDeviceChangePropertyAddress,
      { kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster }
    };

    return noErr == output_device_listener_->OnDefaultDeviceChanged(
        kAudioObjectSystemObject, 1, addresses, output_device_listener_.get());
  }

  MOCK_METHOD0(OnDeviceChange, void());

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<AudioDeviceListenerMac> output_device_listener_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceListenerMacTest);
};

// Simulate a device change events and ensure we get the right callbacks.
TEST_F(AudioDeviceListenerMacTest, OutputDeviceChange) {
  ASSERT_TRUE(ListenerIsValid());
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange());
  message_loop_.RunUntilIdle();
}

}  // namespace media
