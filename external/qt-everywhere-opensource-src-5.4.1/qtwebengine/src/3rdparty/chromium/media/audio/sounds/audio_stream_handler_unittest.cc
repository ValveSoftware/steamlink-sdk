// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/audio/sounds/audio_stream_handler.h"
#include "media/audio/sounds/test_data.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioStreamHandlerTest : public testing::Test {
 public:
  AudioStreamHandlerTest() {}
  virtual ~AudioStreamHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    audio_manager_.reset(AudioManager::CreateForTesting());

    base::StringPiece data(kTestAudioData, arraysize(kTestAudioData));
    audio_stream_handler_.reset(new AudioStreamHandler(data));
  }

  virtual void TearDown() OVERRIDE {
    audio_stream_handler_.reset();
    audio_manager_.reset();
  }

  AudioStreamHandler* audio_stream_handler() {
    return audio_stream_handler_.get();
  }

  void SetObserverForTesting(AudioStreamHandler::TestObserver* observer) {
    AudioStreamHandler::SetObserverForTesting(observer);
  }

  void SetAudioSourceForTesting(
      AudioOutputStream::AudioSourceCallback* source) {
    AudioStreamHandler::SetAudioSourceForTesting(source);
  }

 private:
  scoped_ptr<AudioManager> audio_manager_;
  scoped_ptr<AudioStreamHandler> audio_stream_handler_;

  base::MessageLoop message_loop_;
};

TEST_F(AudioStreamHandlerTest, Play) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());

  SetObserverForTesting(&observer);

  ASSERT_TRUE(audio_stream_handler()->IsInitialized());
  ASSERT_TRUE(audio_stream_handler()->Play());

  run_loop.Run();

  SetObserverForTesting(NULL);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
  ASSERT_EQ(4, observer.cursor());
}

TEST_F(AudioStreamHandlerTest, ConsecutivePlayRequests) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  SineWaveAudioSource source(CHANNEL_LAYOUT_STEREO, 200.0, 8000);

  SetObserverForTesting(&observer);
  SetAudioSourceForTesting(&source);

  ASSERT_TRUE(audio_stream_handler()->IsInitialized());

  ASSERT_TRUE(audio_stream_handler()->Play());
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&AudioStreamHandler::Play),
                 base::Unretained(audio_stream_handler())),
      base::TimeDelta::FromSeconds(1));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AudioStreamHandler::Stop,
                 base::Unretained(audio_stream_handler())),
      base::TimeDelta::FromSeconds(2));

  run_loop.Run();

  SetObserverForTesting(NULL);
  SetAudioSourceForTesting(NULL);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
}

}  // namespace media
