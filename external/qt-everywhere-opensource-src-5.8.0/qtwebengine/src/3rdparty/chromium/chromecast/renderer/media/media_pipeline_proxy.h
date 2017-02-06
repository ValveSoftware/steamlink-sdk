// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_MEDIA_PIPELINE_PROXY_H_
#define CHROMECAST_RENDERER_MEDIA_MEDIA_PIPELINE_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/serial_runner.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {
class AudioPipelineProxy;
class CodedFrameProvider;
class MediaChannelProxy;
class MediaPipelineProxyInternal;
class VideoPipelineProxy;

class MediaPipelineProxy {
 public:
  MediaPipelineProxy(int render_frame_id,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     LoadType load_type);
  ~MediaPipelineProxy();

  void SetClient(const MediaPipelineClient& client);
  void SetCdm(int cdm_id);
  AudioPipelineProxy* GetAudioPipeline() const;
  VideoPipelineProxy* GetVideoPipeline() const;
  void InitializeAudio(const ::media::AudioDecoderConfig& config,
                       std::unique_ptr<CodedFrameProvider> frame_provider,
                       const ::media::PipelineStatusCB& status_cb);
  void InitializeVideo(const std::vector<::media::VideoDecoderConfig>& configs,
                       std::unique_ptr<CodedFrameProvider> frame_provider,
                       const ::media::PipelineStatusCB& status_cb);
  void StartPlayingFrom(base::TimeDelta time);
  void Flush(const base::Closure& flush_cb);
  void Stop();
  void SetPlaybackRate(double playback_rate);

 private:
  void OnProxyFlushDone(const base::Closure& flush_cb,
                        ::media::PipelineStatus status);

  base::ThreadChecker thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  const int render_frame_id_;

  // CMA channel to convey IPC messages.
  scoped_refptr<MediaChannelProxy> const media_channel_proxy_;

  std::unique_ptr<MediaPipelineProxyInternal> proxy_;

  bool has_audio_;
  bool has_video_;
  std::unique_ptr<AudioPipelineProxy> audio_pipeline_;
  std::unique_ptr<VideoPipelineProxy> video_pipeline_;
  std::unique_ptr<::media::SerialRunner> pending_flush_callbacks_;

  base::WeakPtr<MediaPipelineProxy> weak_this_;
  base::WeakPtrFactory<MediaPipelineProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_MEDIA_PIPELINE_PROXY_H_
