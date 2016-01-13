// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_parameters.h"

#include "base/logging.h"
#include "media/base/limits.h"

namespace media {

AudioParameters::AudioParameters()
    : format_(AUDIO_PCM_LINEAR),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      sample_rate_(0),
      bits_per_sample_(0),
      frames_per_buffer_(0),
      channels_(0),
      input_channels_(0),
      effects_(NO_EFFECTS) {
}

AudioParameters::AudioParameters(Format format, ChannelLayout channel_layout,
                                 int sample_rate, int bits_per_sample,
                                 int frames_per_buffer)
    : format_(format),
      channel_layout_(channel_layout),
      sample_rate_(sample_rate),
      bits_per_sample_(bits_per_sample),
      frames_per_buffer_(frames_per_buffer),
      channels_(ChannelLayoutToChannelCount(channel_layout)),
      input_channels_(0),
      effects_(NO_EFFECTS) {
}

AudioParameters::AudioParameters(Format format, ChannelLayout channel_layout,
                                 int input_channels,
                                 int sample_rate, int bits_per_sample,
                                 int frames_per_buffer, int effects)
    : format_(format),
      channel_layout_(channel_layout),
      sample_rate_(sample_rate),
      bits_per_sample_(bits_per_sample),
      frames_per_buffer_(frames_per_buffer),
      channels_(ChannelLayoutToChannelCount(channel_layout)),
      input_channels_(input_channels),
      effects_(effects) {
}

AudioParameters::AudioParameters(Format format, ChannelLayout channel_layout,
                                 int channels, int input_channels,
                                 int sample_rate, int bits_per_sample,
                                 int frames_per_buffer, int effects)
    : format_(format),
      channel_layout_(channel_layout),
      sample_rate_(sample_rate),
      bits_per_sample_(bits_per_sample),
      frames_per_buffer_(frames_per_buffer),
      channels_(channels),
      input_channels_(input_channels),
      effects_(effects) {
  if (channel_layout != CHANNEL_LAYOUT_DISCRETE)
    DCHECK_EQ(channels, ChannelLayoutToChannelCount(channel_layout));
}

void AudioParameters::Reset(Format format, ChannelLayout channel_layout,
                            int channels, int input_channels,
                            int sample_rate, int bits_per_sample,
                            int frames_per_buffer) {
  if (channel_layout != CHANNEL_LAYOUT_DISCRETE)
    DCHECK_EQ(channels, ChannelLayoutToChannelCount(channel_layout));

  format_ = format;
  channel_layout_ = channel_layout;
  channels_ = channels;
  input_channels_ = input_channels;
  sample_rate_ = sample_rate;
  bits_per_sample_ = bits_per_sample;
  frames_per_buffer_ = frames_per_buffer;
}

bool AudioParameters::IsValid() const {
  return (format_ >= AUDIO_PCM_LINEAR) &&
         (format_ < AUDIO_LAST_FORMAT) &&
         (channels_ > 0) &&
         (channels_ <= media::limits::kMaxChannels) &&
         (channel_layout_ > CHANNEL_LAYOUT_UNSUPPORTED) &&
         (channel_layout_ <= CHANNEL_LAYOUT_MAX) &&
         (input_channels_ >= 0) &&
         (input_channels_ <= media::limits::kMaxChannels) &&
         (sample_rate_ >= media::limits::kMinSampleRate) &&
         (sample_rate_ <= media::limits::kMaxSampleRate) &&
         (bits_per_sample_ > 0) &&
         (bits_per_sample_ <= media::limits::kMaxBitsPerSample) &&
         (frames_per_buffer_ > 0) &&
         (frames_per_buffer_ <= media::limits::kMaxSamplesPerPacket);
}

int AudioParameters::GetBytesPerBuffer() const {
  return frames_per_buffer_ * GetBytesPerFrame();
}

int AudioParameters::GetBytesPerSecond() const {
  return sample_rate_ * GetBytesPerFrame();
}

int AudioParameters::GetBytesPerFrame() const {
  return channels_ * bits_per_sample_ / 8;
}

base::TimeDelta AudioParameters::GetBufferDuration() const {
  return base::TimeDelta::FromMicroseconds(
      frames_per_buffer_ * base::Time::kMicrosecondsPerSecond /
      static_cast<float>(sample_rate_));
}

}  // namespace media
