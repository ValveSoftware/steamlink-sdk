// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_MEDIA_PIPELINE_HOST_H_
#define CHROMECAST_BROWSER_MEDIA_MEDIA_PIPELINE_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromecast/browser/media/media_pipeline_backend_factory.h"
#include "chromecast/common/media/cma_ipc_common.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "media/base/pipeline_status.h"

namespace base {
class SharedMemory;
}

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
class TaskRunnerImpl;

namespace media {
struct AvPipelineClient;
struct MediaPipelineClient;
struct VideoPipelineClient;
class CastCdmContext;
class CodedFrameProvider;
class MediaPipelineImpl;

class MediaPipelineHost {
 public:
  MediaPipelineHost();
  ~MediaPipelineHost();

  void Initialize(LoadType load_type,
                  const MediaPipelineClient& client,
                  const CreateMediaPipelineBackendCB& create_backend_cb);

  void SetAvPipe(TrackId track_id,
                 std::unique_ptr<base::SharedMemory> shared_mem,
                 const base::Closure& pipe_read_activity_cb,
                 const base::Closure& av_pipe_set_cb);
  void AudioInitialize(TrackId track_id,
                       const AvPipelineClient& client,
                       const ::media::AudioDecoderConfig& config,
                       const ::media::PipelineStatusCB& status_cb);
  void VideoInitialize(TrackId track_id,
                       const VideoPipelineClient& client,
                       const std::vector< ::media::VideoDecoderConfig>& configs,
                       const ::media::PipelineStatusCB& status_cb);
  void StartPlayingFrom(base::TimeDelta time);
  void Flush(const base::Closure& flush_cb);
  void Stop();

  void SetPlaybackRate(double playback_rate);
  void SetVolume(TrackId track_id, float playback_rate);
  void SetCdm(CastCdmContext* cdm);

  void NotifyPipeWrite(TrackId track_id);

 private:
  base::ThreadChecker thread_checker_;

  std::unique_ptr<TaskRunnerImpl> task_runner_;
  std::unique_ptr<MediaPipelineImpl> media_pipeline_;

  std::unique_ptr<CodedFrameProvider> audio_frame_provider_;
  std::unique_ptr<CodedFrameProvider> video_frame_provider_;

  // The shared memory for a track id must be valid until Stop is invoked on
  // that track id.
  struct MediaTrackHost;
  typedef std::map<TrackId, MediaTrackHost*> MediaTrackMap;
  MediaTrackMap media_track_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineHost);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_MEDIA_PIPELINE_HOST_H_
