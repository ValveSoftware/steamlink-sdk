// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

class MediaPipelineBackendManager;

class MediaPipelineBackendWrapper : public MediaPipelineBackend {
 public:
  MediaPipelineBackendWrapper(std::unique_ptr<MediaPipelineBackend> backend,
                              int stream_type,
                              float stream_type_volume,
                              MediaPipelineBackendManager* backend_manager);
  ~MediaPipelineBackendWrapper() override;

  // MediaPipelineBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  bool Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() override;
  bool SetPlaybackRate(float rate) override;

  int GetStreamType() const;
  void SetStreamTypeVolume(float stream_type_volume);

 private:
  std::unique_ptr<MediaPipelineBackend> backend_;
  const int stream_type_;
  std::unique_ptr<AudioDecoderWrapper> audio_decoder_wrapper_;
  float stream_type_volume_;
  bool is_initialized_;
  MediaPipelineBackendManager* const backend_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_
