// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_manager.h"
#include "media/audio/win/audio_device_listener_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;

namespace media {

static const char kNoDevice[] = "";
static const char kFirstTestDevice[] = "test_device_0";
static const char kSecondTestDevice[] = "test_device_1";

class AudioDeviceListenerWinTest : public testing::Test {
 public:
  AudioDeviceListenerWinTest()
      : com_init_(ScopedCOMInitializer::kMTA) {
  }

  virtual void SetUp() {
    if (!CoreAudioUtil::IsSupported())
      return;

    output_device_listener_.reset(new AudioDeviceListenerWin(base::Bind(
        &AudioDeviceListenerWinTest::OnDeviceChange, base::Unretained(this))));
  }

  // Simulate a device change where no output devices are available.
  bool SimulateNullDefaultOutputDeviceChange() {
    return output_device_listener_->OnDefaultDeviceChanged(
        static_cast<EDataFlow>(eConsole), static_cast<ERole>(eRender),
        NULL) == S_OK;
  }

  bool SimulateDefaultOutputDeviceChange(const char* new_device_id) {
    return output_device_listener_->OnDefaultDeviceChanged(
        static_cast<EDataFlow>(eConsole), static_cast<ERole>(eRender),
        base::ASCIIToWide(new_device_id).c_str()) == S_OK;
  }

  void SetOutputDeviceId(std::string new_device_id) {
    output_device_listener_->default_render_device_id_ = new_device_id;
  }

  MOCK_METHOD0(OnDeviceChange, void());

 private:
  ScopedCOMInitializer com_init_;
  scoped_ptr<AudioDeviceListenerWin> output_device_listener_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceListenerWinTest);
};

// Simulate a device change events and ensure we get the right callbacks.
TEST_F(AudioDeviceListenerWinTest, OutputDeviceChange) {
  if (!CoreAudioUtil::IsSupported())
    return;

  SetOutputDeviceId(kNoDevice);
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kFirstTestDevice));

  testing::Mock::VerifyAndClear(this);
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kSecondTestDevice));

  // The second device event should be ignored since the device id has not
  // changed.
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kSecondTestDevice));
}

// Ensure that null output device changes don't crash.  Simulates the situation
// where we have no output devices.
TEST_F(AudioDeviceListenerWinTest, NullOutputDeviceChange) {
  if (!CoreAudioUtil::IsSupported())
    return;

  SetOutputDeviceId(kNoDevice);
  EXPECT_CALL(*this, OnDeviceChange()).Times(0);
  ASSERT_TRUE(SimulateNullDefaultOutputDeviceChange());

  testing::Mock::VerifyAndClear(this);
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kFirstTestDevice));

  testing::Mock::VerifyAndClear(this);
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateNullDefaultOutputDeviceChange());
}

}  // namespace media
