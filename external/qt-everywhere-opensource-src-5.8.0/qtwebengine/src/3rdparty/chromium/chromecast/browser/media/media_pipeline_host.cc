// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/media_pipeline_host.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/shared_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/common/media/shared_memory_chunk.h"
#include "chromecast/media/base/media_caps.h"
#include "chromecast/media/cdm/cast_cdm_context.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/ipc_streamer/coded_frame_provider_host.h"
#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

struct MediaPipelineHost::MediaTrackHost {
  MediaTrackHost();
  ~MediaTrackHost();

  base::Closure pipe_write_cb;
};

MediaPipelineHost::MediaTrackHost::MediaTrackHost() {
}

MediaPipelineHost::MediaTrackHost::~MediaTrackHost() {
}

MediaPipelineHost::MediaPipelineHost() {
  thread_checker_.DetachFromThread();
}

MediaPipelineHost::~MediaPipelineHost() {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (MediaTrackMap::iterator it = media_track_map_.begin();
       it != media_track_map_.end(); ++it) {
    std::unique_ptr<MediaTrackHost> media_track(it->second);
  }
  media_track_map_.clear();
}

void MediaPipelineHost::Initialize(
    LoadType load_type,
    const MediaPipelineClient& client,
    const CreateMediaPipelineBackendCB& create_backend_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_.reset(new MediaPipelineImpl());
  task_runner_.reset(new TaskRunnerImpl());
  MediaPipelineDeviceParams::MediaSyncType sync_type =
      (load_type == kLoadTypeMediaStream)
          ? MediaPipelineDeviceParams::kModeIgnorePts
          : MediaPipelineDeviceParams::kModeSyncPts;
  MediaPipelineDeviceParams default_parameters(sync_type, task_runner_.get());

  media_pipeline_->SetClient(client);
  media_pipeline_->Initialize(load_type,
                              create_backend_cb.Run(default_parameters));
}

void MediaPipelineHost::SetAvPipe(
    TrackId track_id,
    std::unique_ptr<base::SharedMemory> shared_mem,
    const base::Closure& pipe_read_activity_cb,
    const base::Closure& av_pipe_set_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kAudioTrackId || track_id == kVideoTrackId);

  size_t shared_mem_size = shared_mem->requested_size();
  std::unique_ptr<MediaMemoryChunk> shared_memory_chunk(
      new SharedMemoryChunk(std::move(shared_mem), shared_mem_size));
  std::unique_ptr<MediaMessageFifo> media_message_fifo(
      new MediaMessageFifo(std::move(shared_memory_chunk), shared_mem_size));
  media_message_fifo->ObserveReadActivity(pipe_read_activity_cb);
  std::unique_ptr<CodedFrameProviderHost> frame_provider_host(
      new CodedFrameProviderHost(std::move(media_message_fifo)));

  MediaTrackMap::iterator it = media_track_map_.find(track_id);
  MediaTrackHost* media_track_host;
  if (it == media_track_map_.end()) {
    media_track_host = new MediaTrackHost();
    media_track_map_.insert(
        std::pair<TrackId, MediaTrackHost*>(track_id, media_track_host));
  } else {
    media_track_host = it->second;
  }
  media_track_host->pipe_write_cb = frame_provider_host->GetFifoWriteEventCb();

  if (track_id == kAudioTrackId) {
    audio_frame_provider_ = std::move(frame_provider_host);
  } else {
    video_frame_provider_ = std::move(frame_provider_host);
  }
  av_pipe_set_cb.Run();
}

void MediaPipelineHost::AudioInitialize(
    TrackId track_id,
    const AvPipelineClient& client,
    const ::media::AudioDecoderConfig& config,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kAudioTrackId);
  ::media::PipelineStatus status = media_pipeline_->InitializeAudio(
      config, client, std::move(audio_frame_provider_));
  status_cb.Run(status);
}

void MediaPipelineHost::VideoInitialize(
    TrackId track_id,
    const VideoPipelineClient& client,
    const std::vector< ::media::VideoDecoderConfig>& configs,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kVideoTrackId);
  ::media::PipelineStatus status = media_pipeline_->InitializeVideo(
      configs, client, std::move(video_frame_provider_));
  status_cb.Run(status);
}

void MediaPipelineHost::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->StartPlayingFrom(time);
}

void MediaPipelineHost::Flush(const base::Closure& flush_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->Flush(flush_cb);
}

void MediaPipelineHost::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->Stop();
}

void MediaPipelineHost::SetPlaybackRate(double playback_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->SetPlaybackRate(playback_rate);
}

void MediaPipelineHost::SetVolume(TrackId track_id, float volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kAudioTrackId);
  media_pipeline_->SetVolume(volume);
}

void MediaPipelineHost::SetCdm(CastCdmContext* cdm) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->SetCdm(cdm);
}

void MediaPipelineHost::NotifyPipeWrite(TrackId track_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MediaTrackMap::iterator it = media_track_map_.find(track_id);
  if (it == media_track_map_.end())
    return;

  MediaTrackHost* media_track_host = it->second;
  if (!media_track_host->pipe_write_cb.is_null())
    media_track_host->pipe_write_cb.Run();
}

}  // namespace media
}  // namespace chromecast
