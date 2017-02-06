// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_STATUS_H_
#define MEDIA_BASE_PIPELINE_STATUS_H_

#include "base/callback.h"

#include <stdint.h>

#include <string>

namespace media {

// Status states for pipeline.  All codes except PIPELINE_OK indicate errors.
// Logged to UMA, so never reuse a value, always add new/greater ones!
enum PipelineStatus {
  PIPELINE_OK = 0,
  // Deprecated: PIPELINE_ERROR_URL_NOT_FOUND = 1,
  PIPELINE_ERROR_NETWORK = 2,
  PIPELINE_ERROR_DECODE = 3,
  // Deprecated: PIPELINE_ERROR_DECRYPT = 4,
  PIPELINE_ERROR_ABORT = 5,
  PIPELINE_ERROR_INITIALIZATION_FAILED = 6,
  PIPELINE_ERROR_COULD_NOT_RENDER = 8,
  PIPELINE_ERROR_READ = 9,
  // Deprecated: PIPELINE_ERROR_OPERATION_PENDING = 10,
  PIPELINE_ERROR_INVALID_STATE = 11,

  // Demuxer related errors.
  DEMUXER_ERROR_COULD_NOT_OPEN = 12,
  DEMUXER_ERROR_COULD_NOT_PARSE = 13,
  DEMUXER_ERROR_NO_SUPPORTED_STREAMS = 14,

  // Decoder related errors.
  DECODER_ERROR_NOT_SUPPORTED = 15,

  // ChunkDemuxer related errors.
  CHUNK_DEMUXER_ERROR_APPEND_FAILED = 16,
  CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR = 17,
  CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR = 18,

  // Audio rendering errors.
  AUDIO_RENDERER_ERROR = 19,
  AUDIO_RENDERER_ERROR_SPLICE_FAILED = 20,

  // Must be equal to the largest value ever logged.
  PIPELINE_STATUS_MAX = AUDIO_RENDERER_ERROR_SPLICE_FAILED,
};

typedef base::Callback<void(PipelineStatus)> PipelineStatusCB;

struct PipelineStatistics {
  uint64_t audio_bytes_decoded = 0;
  uint64_t video_bytes_decoded = 0;
  uint32_t video_frames_decoded = 0;
  uint32_t video_frames_dropped = 0;
  int64_t audio_memory_usage = 0;
  int64_t video_memory_usage = 0;
};

// Used for updating pipeline statistics; the passed value should be a delta
// of all attributes since the last update.
typedef base::Callback<void(const PipelineStatistics&)> StatisticsCB;

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_STATUS_H_
