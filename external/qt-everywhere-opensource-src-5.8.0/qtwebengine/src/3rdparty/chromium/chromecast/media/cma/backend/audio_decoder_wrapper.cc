// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"

#include "base/logging.h"

namespace chromecast {
namespace media {

AudioDecoderWrapper::AudioDecoderWrapper(
    MediaPipelineBackend::AudioDecoder* audio_decoder)
    : audio_decoder_(audio_decoder),
      stream_type_volume_(1.0),
      stream_volume_(1.0) {
  DCHECK(audio_decoder_);
}

AudioDecoderWrapper::~AudioDecoderWrapper() {
}

void AudioDecoderWrapper::SetDelegate(Delegate* delegate) {
  audio_decoder_->SetDelegate(delegate);
}

MediaPipelineBackend::BufferStatus AudioDecoderWrapper::PushBuffer(
    CastDecoderBuffer* buffer) {
  return audio_decoder_->PushBuffer(buffer);
}

void AudioDecoderWrapper::GetStatistics(Statistics* statistics) {
  audio_decoder_->GetStatistics(statistics);
}

bool AudioDecoderWrapper::SetConfig(const AudioConfig& config) {
  return audio_decoder_->SetConfig(config);
}

bool AudioDecoderWrapper::SetVolume(float multiplier) {
  stream_volume_ = multiplier;
  return audio_decoder_->SetVolume(stream_volume_ * stream_type_volume_);
}

AudioDecoderWrapper::RenderingDelay AudioDecoderWrapper::GetRenderingDelay() {
  return audio_decoder_->GetRenderingDelay();
}

bool AudioDecoderWrapper::SetStreamTypeVolume(float stream_type_volume) {
  stream_type_volume_ = stream_type_volume;
  return audio_decoder_->SetVolume(stream_volume_ * stream_type_volume_);
}

}  // namespace media
}  // namespace chromecast
