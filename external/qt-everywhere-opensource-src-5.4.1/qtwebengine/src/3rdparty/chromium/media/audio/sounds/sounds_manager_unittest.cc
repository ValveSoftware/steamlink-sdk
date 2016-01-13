// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "media/audio/audio_manager.h"
#include "media/audio/sounds/audio_stream_handler.h"
#include "media/audio/sounds/sounds_manager.h"
#include "media/audio/sounds/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class SoundsManagerTest : public testing::Test {
 public:
  SoundsManagerTest() {}
  virtual ~SoundsManagerTest() {}

  virtual void SetUp() OVERRIDE {
    audio_manager_.reset(AudioManager::CreateForTesting());
    SoundsManager::Create();
  }

  virtual void TearDown() OVERRIDE {
    SoundsManager::Shutdown();
    audio_manager_.reset();
  }

  void SetObserverForTesting(AudioStreamHandler::TestObserver* observer) {
    AudioStreamHandler::SetObserverForTesting(observer);
  }

 private:
  scoped_ptr<AudioManager> audio_manager_;

  base::MessageLoop message_loop_;
};

TEST_F(SoundsManagerTest, Play) {
  ASSERT_TRUE(SoundsManager::Get());

  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());

  SetObserverForTesting(&observer);

  ASSERT_TRUE(SoundsManager::Get()->Initialize(
      kTestAudioKey,
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData))));
  ASSERT_EQ(20,
            SoundsManager::Get()->GetDuration(kTestAudioKey).InMicroseconds());
  ASSERT_TRUE(SoundsManager::Get()->Play(kTestAudioKey));
  run_loop.Run();

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
  ASSERT_EQ(4, observer.cursor());

  SetObserverForTesting(NULL);
}

}  // namespace media
