// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/logging.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Validate that the SineWaveAudioSource writes the expected values.
TEST(SimpleSources, SineWaveAudioSource) {
  static const uint32 samples = 1024;
  static const uint32 bytes_per_sample = 2;
  static const int freq = 200;

  AudioParameters params(
        AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
        AudioParameters::kTelephoneSampleRate, bytes_per_sample * 8, samples);

  SineWaveAudioSource source(1, freq, params.sample_rate());
  scoped_ptr<AudioBus> audio_bus = AudioBus::Create(params);
  source.OnMoreData(audio_bus.get(), AudioBuffersState());
  EXPECT_EQ(1, source.callbacks());
  EXPECT_EQ(0, source.errors());

  uint32 half_period = AudioParameters::kTelephoneSampleRate / (freq * 2);

  // Spot test positive incursion of sine wave.
  EXPECT_NEAR(0, audio_bus->channel(0)[0],
              std::numeric_limits<float>::epsilon());
  EXPECT_FLOAT_EQ(0.15643446f, audio_bus->channel(0)[1]);
  EXPECT_LT(audio_bus->channel(0)[1], audio_bus->channel(0)[2]);
  EXPECT_LT(audio_bus->channel(0)[2], audio_bus->channel(0)[3]);
  // Spot test negative incursion of sine wave.
  EXPECT_NEAR(0, audio_bus->channel(0)[half_period],
              std::numeric_limits<float>::epsilon());
  EXPECT_FLOAT_EQ(-0.15643446f, audio_bus->channel(0)[half_period + 1]);
  EXPECT_GT(audio_bus->channel(0)[half_period + 1],
            audio_bus->channel(0)[half_period + 2]);
  EXPECT_GT(audio_bus->channel(0)[half_period + 2],
            audio_bus->channel(0)[half_period + 3]);
}

TEST(SimpleSources, SineWaveAudioCapped) {
  SineWaveAudioSource source(1, 200, AudioParameters::kTelephoneSampleRate);

  static const int kSampleCap = 100;
  source.CapSamples(kSampleCap);

  scoped_ptr<AudioBus> audio_bus = AudioBus::Create(1, 2 * kSampleCap);
  EXPECT_EQ(source.OnMoreData(
      audio_bus.get(), AudioBuffersState()), kSampleCap);
  EXPECT_EQ(1, source.callbacks());
  EXPECT_EQ(source.OnMoreData(audio_bus.get(), AudioBuffersState()), 0);
  EXPECT_EQ(2, source.callbacks());
  source.Reset();
  EXPECT_EQ(source.OnMoreData(
      audio_bus.get(), AudioBuffersState()), kSampleCap);
  EXPECT_EQ(3, source.callbacks());
  EXPECT_EQ(0, source.errors());
}

TEST(SimpleSources, OnError) {
  SineWaveAudioSource source(1, 200, AudioParameters::kTelephoneSampleRate);
  source.OnError(NULL);
  EXPECT_EQ(1, source.errors());
  source.OnError(NULL);
  EXPECT_EQ(2, source.errors());
}

}  // namespace media
