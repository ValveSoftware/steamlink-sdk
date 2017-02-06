// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_hardware_config.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "build/build_config.h"

using base::AutoLock;
using media::AudioParameters;

namespace media {

AudioHardwareConfig::AudioHardwareConfig(
    const AudioParameters& input_params,
    const AudioParameters& output_params)
    : input_params_(input_params),
      output_params_(output_params) {}

AudioHardwareConfig::~AudioHardwareConfig() {}

int AudioHardwareConfig::GetOutputBufferSize() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.frames_per_buffer();
}

int AudioHardwareConfig::GetOutputSampleRate() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.sample_rate();
}

ChannelLayout AudioHardwareConfig::GetOutputChannelLayout() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.channel_layout();
}

int AudioHardwareConfig::GetOutputChannels() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.channels();
}

int AudioHardwareConfig::GetInputSampleRate() const {
  AutoLock auto_lock(config_lock_);
  return input_params_.sample_rate();
}

ChannelLayout AudioHardwareConfig::GetInputChannelLayout() const {
  AutoLock auto_lock(config_lock_);
  return input_params_.channel_layout();
}

int AudioHardwareConfig::GetInputChannels() const {
  AutoLock auto_lock(config_lock_);
  return input_params_.channels();
}

media::AudioParameters
AudioHardwareConfig::GetInputConfig() const {
  AutoLock auto_lock(config_lock_);
  return input_params_;
}

media::AudioParameters
AudioHardwareConfig::GetOutputConfig() const {
  AutoLock auto_lock(config_lock_);
  return output_params_;
}

void AudioHardwareConfig::UpdateInputConfig(
    const AudioParameters& input_params) {
  AutoLock auto_lock(config_lock_);
  input_params_ = input_params;
}

void AudioHardwareConfig::UpdateOutputConfig(
    const AudioParameters& output_params) {
  AutoLock auto_lock(config_lock_);
  output_params_ = output_params;
}

}  // namespace media
