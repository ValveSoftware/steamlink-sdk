// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_DECODER_ALSA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_DECODER_ALSA_H_

#include "base/bind.h"
#include "base/location.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa_input.h"
#include "chromecast/media/cma/decoder/cast_audio_decoder.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace chromecast {
namespace media {
class DecoderBufferBase;
class MediaPipelineBackendAlsa;

class AudioDecoderAlsa : public MediaPipelineBackend::AudioDecoder,
                         public StreamMixerAlsaInput::Delegate {
 public:
  using BufferStatus = MediaPipelineBackend::BufferStatus;

  explicit AudioDecoderAlsa(MediaPipelineBackendAlsa* backend);
  ~AudioDecoderAlsa() override;

  bool Initialize();
  bool Start(int64_t start_pts);
  bool Stop();
  bool Pause();
  bool Resume();

  int64_t current_pts() const { return current_pts_; }

  // MediaPipelineBackend::AudioDecoder implementation:
  void SetDelegate(
      MediaPipelineBackend::Decoder::Delegate* delegate) override;
  BufferStatus PushBuffer(CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;

 private:
  // StreamMixerAlsaInput::Delegate implementation:
  void OnWritePcmCompletion(BufferStatus status,
                            const RenderingDelay& delay) override;
  void OnMixerError(MixerError error) override;

  void CleanUpPcm();
  void CreateDecoder();
  void OnDecoderInitialized(bool success);
  void OnBufferDecoded(uint64_t input_bytes,
                       CastAudioDecoder::Status status,
                       const scoped_refptr<DecoderBufferBase>& decoded);
  void RunEos();
  bool BypassDecoder() const;
  bool ShouldStartClock() const;
  void UpdateStatistics(Statistics delta);

  MediaPipelineBackendAlsa* const backend_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  MediaPipelineBackend::Decoder::Delegate* delegate_;

  Statistics stats_;
  bool is_eos_;
  bool error_;

  AudioConfig config_;
  std::unique_ptr<CastAudioDecoder> decoder_;

  int64_t current_pts_;
  int64_t last_buffer_pts_;

  std::unique_ptr<StreamMixerAlsaInput> mixer_input_;
  RenderingDelay last_known_delay_;
  float volume_multiplier_;

  base::WeakPtrFactory<AudioDecoderAlsa> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderAlsa);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_DECODER_ALSA_H_
