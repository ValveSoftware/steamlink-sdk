// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/fake_audio_consumer.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kTestCallbacks = 5;

class FakeAudioConsumerTest : public testing::Test {
 public:
  FakeAudioConsumerTest()
      : params_(
            AudioParameters::AUDIO_FAKE, CHANNEL_LAYOUT_STEREO, 44100, 8, 128),
        fake_consumer_(message_loop_.message_loop_proxy(), params_),
        source_(params_.channels(), 200.0, params_.sample_rate()) {
    time_between_callbacks_ = base::TimeDelta::FromMicroseconds(
        params_.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
        static_cast<float>(params_.sample_rate()));
  }

  virtual ~FakeAudioConsumerTest() {}

  void ConsumeData(AudioBus* audio_bus) {
    source_.OnMoreData(audio_bus, AudioBuffersState());
  }

  void RunOnAudioThread() {
    ASSERT_TRUE(message_loop_.message_loop_proxy()->BelongsToCurrentThread());
    fake_consumer_.Start(base::Bind(
        &FakeAudioConsumerTest::ConsumeData, base::Unretained(this)));
  }

  void RunOnceOnAudioThread() {
    ASSERT_TRUE(message_loop_.message_loop_proxy()->BelongsToCurrentThread());
    RunOnAudioThread();
    // Start() should immediately post a task to run the source callback, so we
    // should end up with only a single callback being run.
    message_loop_.PostTask(FROM_HERE, base::Bind(
        &FakeAudioConsumerTest::EndTest, base::Unretained(this), 1));
  }

  void StopStartOnAudioThread() {
    ASSERT_TRUE(message_loop_.message_loop_proxy()->BelongsToCurrentThread());
    fake_consumer_.Stop();
    RunOnAudioThread();
  }

  void TimeCallbacksOnAudioThread(int callbacks) {
    ASSERT_TRUE(message_loop_.message_loop_proxy()->BelongsToCurrentThread());

    if (source_.callbacks() == 0) {
      RunOnAudioThread();
      start_time_ = base::TimeTicks::Now();
    }

    // Keep going until we've seen the requested number of callbacks.
    if (source_.callbacks() < callbacks) {
      message_loop_.PostDelayedTask(FROM_HERE, base::Bind(
          &FakeAudioConsumerTest::TimeCallbacksOnAudioThread,
          base::Unretained(this), callbacks), time_between_callbacks_ / 2);
    } else {
      end_time_ = base::TimeTicks::Now();
      EndTest(callbacks);
    }
  }

  void EndTest(int callbacks) {
    ASSERT_TRUE(message_loop_.message_loop_proxy()->BelongsToCurrentThread());
    fake_consumer_.Stop();
    EXPECT_LE(callbacks, source_.callbacks());
    message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

 protected:
  base::MessageLoop message_loop_;
  AudioParameters params_;
  FakeAudioConsumer fake_consumer_;
  SineWaveAudioSource source_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
  base::TimeDelta time_between_callbacks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioConsumerTest);
};

// Ensure the fake audio stream runs on the audio thread and handles fires
// callbacks to the AudioSourceCallback.
TEST_F(FakeAudioConsumerTest, FakeStreamBasicCallback) {
  message_loop_.PostTask(FROM_HERE, base::Bind(
      &FakeAudioConsumerTest::RunOnceOnAudioThread,
      base::Unretained(this)));
  message_loop_.Run();
}

// Ensure the time between callbacks is sane.
TEST_F(FakeAudioConsumerTest, TimeBetweenCallbacks) {
  message_loop_.PostTask(FROM_HERE, base::Bind(
      &FakeAudioConsumerTest::TimeCallbacksOnAudioThread,
      base::Unretained(this), kTestCallbacks));
  message_loop_.Run();

  // There are only (kTestCallbacks - 1) intervals between kTestCallbacks.
  base::TimeDelta actual_time_between_callbacks =
      (end_time_ - start_time_) / (source_.callbacks() - 1);

  // Ensure callback time is no faster than the expected time between callbacks.
  EXPECT_TRUE(actual_time_between_callbacks >= time_between_callbacks_);

  // Softly check if the callback time is no slower than twice the expected time
  // between callbacks.  Since this test runs on the bots we can't be too strict
  // with the bounds.
  if (actual_time_between_callbacks > 2 * time_between_callbacks_)
    LOG(ERROR) << "Time between fake audio callbacks is too large!";
}

// Ensure Start()/Stop() on the stream doesn't generate too many callbacks.  See
// http://crbug.com/159049
TEST_F(FakeAudioConsumerTest, StartStopClearsCallbacks) {
  message_loop_.PostTask(FROM_HERE, base::Bind(
      &FakeAudioConsumerTest::TimeCallbacksOnAudioThread,
      base::Unretained(this), kTestCallbacks));

  // Issue a Stop() / Start() in between expected callbacks to maximize the
  // chance of catching the FakeAudioOutputStream doing the wrong thing.
  message_loop_.PostDelayedTask(FROM_HERE, base::Bind(
      &FakeAudioConsumerTest::StopStartOnAudioThread,
      base::Unretained(this)), time_between_callbacks_ / 2);

  // EndTest() will ensure the proper number of callbacks have occurred.
  message_loop_.Run();
}

}  // namespace media
