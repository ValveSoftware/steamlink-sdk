// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_METADATA_H_
#define MEDIA_BASE_PIPELINE_METADATA_H_

#include "base/time/time.h"
#include "media/base/video_rotation.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Metadata describing a pipeline once it has been initialized.
struct PipelineMetadata {
  PipelineMetadata()
      : has_audio(false), has_video(false), video_rotation(VIDEO_ROTATION_0) {}

  bool has_audio;
  bool has_video;
  gfx::Size natural_size;
  VideoRotation video_rotation;
  base::Time timeline_offset;
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_METADATA_H_
