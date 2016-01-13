// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "media/base/audio_hardware_config.h"
#include "media/audio/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kOutputBufferSize = 2048;
static const int kOutputSampleRate = 48000;
static const ChannelLayout kOutputChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kInputSampleRate = 44100;
static const ChannelLayout kInputChannelLayout = CHANNEL_LAYOUT_STEREO;

TEST(AudioHardwareConfig, Getters) {
  AudioParameters input_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      kInputChannelLayout,
      kInputSampleRate,
      16,
      kOutputBufferSize);

  AudioParameters output_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      kOutputChannelLayout,
      kOutputSampleRate,
      16,
      kOutputBufferSize);

  AudioHardwareConfig fake_config(input_params, output_params);

  EXPECT_EQ(kOutputBufferSize, fake_config.GetOutputBufferSize());
  EXPECT_EQ(kOutputSampleRate, fake_config.GetOutputSampleRate());
  EXPECT_EQ(kInputSampleRate, fake_config.GetInputSampleRate());
  EXPECT_EQ(kInputChannelLayout, fake_config.GetInputChannelLayout());
}

TEST(AudioHardwareConfig, Setters) {
  AudioParameters input_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      kInputChannelLayout,
      kInputSampleRate,
      16,
      kOutputBufferSize);

  AudioParameters output_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      kOutputChannelLayout,
      kOutputSampleRate,
      16,
      kOutputBufferSize);

  AudioHardwareConfig fake_config(input_params, output_params);

  // Verify output parameters.
  const int kNewOutputBufferSize = kOutputBufferSize * 2;
  const int kNewOutputSampleRate = kOutputSampleRate * 2;
  EXPECT_NE(kNewOutputBufferSize, fake_config.GetOutputBufferSize());
  EXPECT_NE(kNewOutputSampleRate, fake_config.GetOutputSampleRate());

  AudioParameters new_output_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      kOutputChannelLayout,
      kNewOutputSampleRate,
      16,
      kNewOutputBufferSize);
  fake_config.UpdateOutputConfig(new_output_params);
  EXPECT_EQ(kNewOutputBufferSize, fake_config.GetOutputBufferSize());
  EXPECT_EQ(kNewOutputSampleRate, fake_config.GetOutputSampleRate());

  // Verify input parameters.
  const int kNewInputSampleRate = kInputSampleRate * 2;
  const ChannelLayout kNewInputChannelLayout = CHANNEL_LAYOUT_MONO;
  EXPECT_NE(kNewInputSampleRate, fake_config.GetInputSampleRate());
  EXPECT_NE(kNewInputChannelLayout, fake_config.GetInputChannelLayout());

  AudioParameters new_input_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      kNewInputChannelLayout,
      kNewInputSampleRate,
      16,
      kOutputBufferSize);
  fake_config.UpdateInputConfig(new_input_params);
  EXPECT_EQ(kNewInputSampleRate, fake_config.GetInputSampleRate());
  EXPECT_EQ(kNewInputChannelLayout, fake_config.GetInputChannelLayout());
}

TEST(AudioHardwareConfig, HighLatencyBufferSizes) {
  AudioParameters input_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               kInputChannelLayout,
                               kInputSampleRate,
                               16,
                               kOutputBufferSize);
  AudioParameters output_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                kOutputChannelLayout,
                                3200,
                                16,
                                32);
  AudioHardwareConfig fake_config(input_params, output_params);

#if defined(OS_WIN)
  for (int i = 6400; i <= 204800; i *= 2) {
    fake_config.UpdateOutputConfig(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        kOutputChannelLayout,
                        i,
                        16,
                        i / 100));
    EXPECT_EQ(2 * (i / 100), fake_config.GetHighLatencyBufferSize());
  }
#else
  EXPECT_EQ(64, fake_config.GetHighLatencyBufferSize());

  for (int i = 6400; i <= 204800; i *= 2) {
    fake_config.UpdateOutputConfig(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        kOutputChannelLayout,
                        i,
                        16,
                        32));
    EXPECT_EQ(2 * (i / 100), fake_config.GetHighLatencyBufferSize());
  }
#endif  // defined(OS_WIN)
}

}  // namespace content
