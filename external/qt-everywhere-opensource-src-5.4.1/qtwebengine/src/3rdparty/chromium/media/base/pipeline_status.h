// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_STATUS_H_
#define MEDIA_BASE_PIPELINE_STATUS_H_

#include "base/callback.h"

#include <string>

namespace media {

// Status states for pipeline.  All codes except PIPELINE_OK indicate errors.
// Logged to UMA, so never reuse a value, always add new/greater ones!
// TODO(vrk/scherkus): Trim the unused status codes. (crbug.com/126070)
enum PipelineStatus {
  PIPELINE_OK = 0,
  PIPELINE_ERROR_URL_NOT_FOUND = 1,
  PIPELINE_ERROR_NETWORK = 2,
  PIPELINE_ERROR_DECODE = 3,
  PIPELINE_ERROR_DECRYPT = 4,
  PIPELINE_ERROR_ABORT = 5,
  PIPELINE_ERROR_INITIALIZATION_FAILED = 6,
  PIPELINE_ERROR_COULD_NOT_RENDER = 8,
  PIPELINE_ERROR_READ = 9,
  PIPELINE_ERROR_OPERATION_PENDING = 10,
  PIPELINE_ERROR_INVALID_STATE = 11,
  // Demuxer related errors.
  DEMUXER_ERROR_COULD_NOT_OPEN = 12,
  DEMUXER_ERROR_COULD_NOT_PARSE = 13,
  DEMUXER_ERROR_NO_SUPPORTED_STREAMS = 14,
  // Decoder related errors.
  DECODER_ERROR_NOT_SUPPORTED = 15,
  // Must be equal to the largest value ever logged.
  PIPELINE_STATUS_MAX = DECODER_ERROR_NOT_SUPPORTED,
};

typedef base::Callback<void(PipelineStatus)> PipelineStatusCB;

// TODO(scherkus): this should be moved alongside host interface definitions.
struct PipelineStatistics {
  PipelineStatistics()
      : audio_bytes_decoded(0),
        video_bytes_decoded(0),
        video_frames_decoded(0),
        video_frames_dropped(0) {
  }

  uint32 audio_bytes_decoded;  // Should be uint64?
  uint32 video_bytes_decoded;  // Should be uint64?
  uint32 video_frames_decoded;
  uint32 video_frames_dropped;
};

// Used for updating pipeline statistics.
typedef base::Callback<void(const PipelineStatistics&)> StatisticsCB;

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_STATUS_H_
