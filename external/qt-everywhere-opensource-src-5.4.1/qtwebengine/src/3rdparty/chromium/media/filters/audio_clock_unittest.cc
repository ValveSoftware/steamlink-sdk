// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "media/filters/audio_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioClockTest : public testing::Test {
 public:
  AudioClockTest()
      : sample_rate_(10),
        timestamp_helper_(sample_rate_),
        clock_(sample_rate_) {
    timestamp_helper_.SetBaseTimestamp(base::TimeDelta());
  }

  virtual ~AudioClockTest() {}

  void WroteAudio(int frames, int delay_frames, float playback_rate) {
    timestamp_helper_.AddFrames(static_cast<int>(frames * playback_rate));
    clock_.WroteAudio(
        frames, delay_frames, playback_rate, timestamp_helper_.GetTimestamp());
  }

  void WroteSilence(int frames, int delay_frames) {
    clock_.WroteSilence(frames, delay_frames);
  }

  int CurrentMediaTimestampInMilliseconds() {
    return clock_.CurrentMediaTimestamp().InMilliseconds();
  }

  int LastEndpointTimestampInMilliseconds() {
    return clock_.last_endpoint_timestamp().InMilliseconds();
  }

  const int sample_rate_;
  AudioTimestampHelper timestamp_helper_;
  AudioClock clock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioClockTest);
};

TEST_F(AudioClockTest, TimestampsStartAtNoTimestamp) {
  EXPECT_EQ(kNoTimestamp(), clock_.CurrentMediaTimestamp());
  EXPECT_EQ(kNoTimestamp(), clock_.last_endpoint_timestamp());
}

TEST_F(AudioClockTest, Playback) {
  // The first time we write data we should expect a negative time matching the
  // current delay.
  WroteAudio(10, 20, 1.0);
  EXPECT_EQ(-2000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(1000, LastEndpointTimestampInMilliseconds());

  // The media time should keep advancing as we write data.
  WroteAudio(10, 20, 1.0);
  EXPECT_EQ(-1000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(2000, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 1.0);
  EXPECT_EQ(0, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(3000, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 1.0);
  EXPECT_EQ(1000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(4000, LastEndpointTimestampInMilliseconds());

  // Introduce a rate change to slow down time. Current time will keep advancing
  // by one second until it hits the slowed down audio.
  WroteAudio(10, 20, 0.5);
  EXPECT_EQ(2000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(4500, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 0.5);
  EXPECT_EQ(3000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(5000, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 0.5);
  EXPECT_EQ(4000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(5500, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 0.5);
  EXPECT_EQ(4500, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(6000, LastEndpointTimestampInMilliseconds());

  // Introduce a rate change to speed up time. Current time will keep advancing
  // by half a second until it hits the the sped up audio.
  WroteAudio(10, 20, 2);
  EXPECT_EQ(5000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(8000, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 2);
  EXPECT_EQ(5500, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(10000, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 2);
  EXPECT_EQ(6000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(12000, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 20, 2);
  EXPECT_EQ(8000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(14000, LastEndpointTimestampInMilliseconds());

  // Write silence to simulate reaching end of stream.
  WroteSilence(10, 20);
  EXPECT_EQ(10000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(14000, LastEndpointTimestampInMilliseconds());

  WroteSilence(10, 20);
  EXPECT_EQ(12000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(14000, LastEndpointTimestampInMilliseconds());

  WroteSilence(10, 20);
  EXPECT_EQ(14000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(14000, LastEndpointTimestampInMilliseconds());

  // At this point media time should stop increasing.
  WroteSilence(10, 20);
  EXPECT_EQ(14000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(14000, LastEndpointTimestampInMilliseconds());
}

TEST_F(AudioClockTest, AlternatingAudioAndSilence) {
  // Buffer #1: [0, 1000)
  WroteAudio(10, 20, 1.0);
  EXPECT_EQ(-2000, CurrentMediaTimestampInMilliseconds());

  // Buffer #2: 1000ms of silence
  WroteSilence(10, 20);
  EXPECT_EQ(-1000, CurrentMediaTimestampInMilliseconds());

  // Buffer #3: [1000, 2000), buffer #1 is at front
  WroteAudio(10, 20, 1.0);
  EXPECT_EQ(0, CurrentMediaTimestampInMilliseconds());

  // Buffer #4: 1000ms of silence, time shouldn't advance
  WroteSilence(10, 20);
  EXPECT_EQ(0, CurrentMediaTimestampInMilliseconds());

  // Buffer #5: [2000, 3000), buffer #3 is at front
  WroteAudio(10, 20, 1.0);
  EXPECT_EQ(1000, CurrentMediaTimestampInMilliseconds());
}

TEST_F(AudioClockTest, ZeroDelay) {
  // The first time we write data we should expect the first timestamp
  // immediately.
  WroteAudio(10, 0, 1.0);
  EXPECT_EQ(0, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(1000, LastEndpointTimestampInMilliseconds());

  // Ditto for all subsequent buffers.
  WroteAudio(10, 0, 1.0);
  EXPECT_EQ(1000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(2000, LastEndpointTimestampInMilliseconds());

  WroteAudio(10, 0, 1.0);
  EXPECT_EQ(2000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(3000, LastEndpointTimestampInMilliseconds());

  // Ditto for silence.
  WroteSilence(10, 0);
  EXPECT_EQ(3000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(3000, LastEndpointTimestampInMilliseconds());

  WroteSilence(10, 0);
  EXPECT_EQ(3000, CurrentMediaTimestampInMilliseconds());
  EXPECT_EQ(3000, LastEndpointTimestampInMilliseconds());
}

}  // namespace media
