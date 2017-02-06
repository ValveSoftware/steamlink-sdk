// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_WRAPPER_H_

#include "base/macros.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

class AudioDecoderWrapper : public MediaPipelineBackend::AudioDecoder {
 public:
  explicit AudioDecoderWrapper(
      MediaPipelineBackend::AudioDecoder* audio_decoder);
  ~AudioDecoderWrapper() override;

  // MediaPipelineBackend::AudioDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;

  bool SetStreamTypeVolume(float stream_type_volume);

 private:
  MediaPipelineBackend::AudioDecoder* const audio_decoder_;
  float stream_type_volume_;
  float stream_volume_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_WRAPPER_H_
