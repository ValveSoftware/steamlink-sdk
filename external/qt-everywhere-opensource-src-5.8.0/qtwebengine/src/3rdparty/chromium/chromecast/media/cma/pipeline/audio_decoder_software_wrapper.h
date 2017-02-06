// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_DECODER_SOFTWARE_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_DECODER_SOFTWARE_WRAPPER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/cma/decoder/cast_audio_decoder.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

// Provides transparent software decoding if the backend does not support the
// incoming audio config.
class AudioDecoderSoftwareWrapper
    : public MediaPipelineBackend::AudioDecoder,
      public MediaPipelineBackend::Decoder::Delegate {
 public:
  using DecoderDelegate = MediaPipelineBackend::Decoder::Delegate;

  AudioDecoderSoftwareWrapper(
      MediaPipelineBackend::AudioDecoder* backend_decoder);
  ~AudioDecoderSoftwareWrapper() override;

  // MediaPipelineBackend::AudioDecoder implementation:
  void SetDelegate(DecoderDelegate* delegate) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;

 private:
  bool CreateSoftwareDecoder(const AudioConfig& config);
  void OnDecoderInitialized(bool success);
  void OnDecodedBuffer(CastAudioDecoder::Status status,
                       const scoped_refptr<DecoderBufferBase>& decoded);

  // MediaPipelineBackend::Decoder::Delegate implementation:
  void OnPushBufferComplete(MediaPipelineBackend::BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override;
  void OnVideoResolutionChanged(const Size& size) override;

  MediaPipelineBackend::AudioDecoder* const backend_decoder_;
  DecoderDelegate* delegate_;
  std::unique_ptr<CastAudioDecoder> software_decoder_;
  AudioConfig output_config_;
  scoped_refptr<DecoderBufferBase> pending_pushed_buffer_;

  base::WeakPtrFactory<AudioDecoderSoftwareWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderSoftwareWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_DECODER_SOFTWARE_WRAPPER_H_
