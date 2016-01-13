// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/mock_audio_source_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;

// TODO(crogers): Most of these tests can be made platform agnostic.
// http://crbug.com/223242

namespace media {

ACTION(ZeroBuffer) {
  arg0->Zero();
}

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class AUHALStreamTest : public testing::Test {
 public:
  AUHALStreamTest()
      : message_loop_(base::MessageLoop::TYPE_UI),
        manager_(AudioManager::CreateForTesting()) {
    // Wait for the AudioManager to finish any initialization on the audio loop.
    base::RunLoop().RunUntilIdle();
  }

  virtual ~AUHALStreamTest() {
    base::RunLoop().RunUntilIdle();
  }

  AudioOutputStream* Create() {
    return manager_->MakeAudioOutputStream(
        manager_->GetDefaultOutputStreamParameters(), "");
  }

  bool CanRunAudioTests() {
    return manager_->HasAudioOutputDevices();
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<AudioManager> manager_;
  MockAudioSourceCallback source_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AUHALStreamTest);
};

TEST_F(AUHALStreamTest, HardwareSampleRate) {
  if (!CanRunAudioTests())
    return;
  const AudioParameters preferred_params =
      manager_->GetDefaultOutputStreamParameters();
  EXPECT_GE(preferred_params.sample_rate(), 16000);
  EXPECT_LE(preferred_params.sample_rate(), 192000);
}

TEST_F(AUHALStreamTest, CreateClose) {
  if (!CanRunAudioTests())
    return;
  Create()->Close();
}

TEST_F(AUHALStreamTest, CreateOpenClose) {
  if (!CanRunAudioTests())
    return;
  AudioOutputStream* stream = Create();
  EXPECT_TRUE(stream->Open());
  stream->Close();
}

TEST_F(AUHALStreamTest, CreateOpenStartStopClose) {
  if (!CanRunAudioTests())
    return;

  AudioOutputStream* stream = Create();
  EXPECT_TRUE(stream->Open());

  // Wait for the first data callback from the OS.
  base::WaitableEvent event(false, false);
  EXPECT_CALL(source_, OnMoreData(_, _))
      .WillOnce(DoAll(ZeroBuffer(), SignalEvent(&event), Return(0)));
  EXPECT_CALL(source_, OnError(_)).Times(0);
  stream->Start(&source_);
  event.Wait();

  stream->Stop();
  stream->Close();
}

}  // namespace media
