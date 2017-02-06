// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"
#include "chromecast/public/cast_media_shlib.h"

namespace chromecast {
namespace media {

MediaPipelineBackendManager::MediaPipelineBackendManager(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)) {
}

MediaPipelineBackendManager::~MediaPipelineBackendManager() {
}

std::unique_ptr<MediaPipelineBackend>
MediaPipelineBackendManager::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  return CreateMediaPipelineBackend(params, 0);
}

std::unique_ptr<MediaPipelineBackend>
MediaPipelineBackendManager::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params,
    int stream_type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<MediaPipelineBackend> backend_ptr(
      new MediaPipelineBackendWrapper(
          base::WrapUnique(
              media::CastMediaShlib::CreateMediaPipelineBackend(params)),
          stream_type, GetVolumeMultiplier(stream_type), this));
  media_pipeline_backends_.push_back(backend_ptr.get());
  return backend_ptr;
}

void MediaPipelineBackendManager::OnMediaPipelineBackendDestroyed(
    const MediaPipelineBackend* backend) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_pipeline_backends_.erase(
      std::remove(media_pipeline_backends_.begin(),
                  media_pipeline_backends_.end(), backend),
      media_pipeline_backends_.end());
}

void MediaPipelineBackendManager::SetVolumeMultiplier(int stream_type,
                                                      float volume) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  volume = std::max(0.0f, std::min(volume, 1.0f));
  volume_by_stream_type_[stream_type] = volume;

  // Set volume for each open media pipeline backends.
  for (auto it = media_pipeline_backends_.begin();
       it != media_pipeline_backends_.end(); it++) {
    MediaPipelineBackendWrapper* wrapper =
        static_cast<MediaPipelineBackendWrapper*>(*it);
    if (wrapper->GetStreamType() == stream_type)
      wrapper->SetStreamTypeVolume(volume);
  }
}

float MediaPipelineBackendManager::GetVolumeMultiplier(int stream_type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  auto it = volume_by_stream_type_.find(stream_type);
  if (it == volume_by_stream_type_.end()) {
    return 1.0;
  } else {
    return it->second;
  }
}

}  // namespace media
}  // namespace chromecast
