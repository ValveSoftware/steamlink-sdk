// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_metadata.h"

namespace media {

PipelineMetadata::PipelineMetadata()
    : has_audio(false), has_video(false), video_rotation(VIDEO_ROTATION_0) {}

PipelineMetadata::~PipelineMetadata() {}

PipelineMetadata::PipelineMetadata(const PipelineMetadata&) = default;

}  // namespace
