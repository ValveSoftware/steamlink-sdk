// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

// This class manages created media pipelines, and provides volume control by
// stream type.
// All functions in this class should be called on the media thread.
class MediaPipelineBackendManager {
 public:
  explicit MediaPipelineBackendManager(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);
  ~MediaPipelineBackendManager();

  // Create media pipeline backend.
  std::unique_ptr<MediaPipelineBackend> CreateMediaPipelineBackend(
      const MediaPipelineDeviceParams& params);

  // Create media pipeline backend with a specific stream_type.
  std::unique_ptr<MediaPipelineBackend> CreateMediaPipelineBackend(
      const MediaPipelineDeviceParams& params,
      int stream_type);

  // Sets the relative volume for a specified stream type,
  // with range [0.0, 1.0] inclusive. If |multiplier| is outside the
  // range [0.0, 1.0], it is clamped to that range.
  // TODO(tianyuwang): change stream_type to use a enum.
  void SetVolumeMultiplier(int stream_type, float volume);

  base::SingleThreadTaskRunner* task_runner() const {
    return media_task_runner_.get();
  }

 private:
  friend class MediaPipelineBackendWrapper;

  // Internal clean up when a new media pipeline backend is destroyed.
  void OnMediaPipelineBackendDestroyed(const MediaPipelineBackend* backend);

  float GetVolumeMultiplier(int stream_type);

  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // A vector that stores all of the existing media_pipeline_backends_.
  std::vector<MediaPipelineBackend*> media_pipeline_backends_;

  // Volume multiplier for each type of audio streams.
  std::map<int, float> volume_by_stream_type_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendManager);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_MANAGER_H_
