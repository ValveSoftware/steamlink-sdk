// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_DEMUXER_STREAM_PLAYER_PARAMS_H_
#define MEDIA_BASE_ANDROID_DEMUXER_STREAM_PLAYER_PARAMS_H_

#include <stdint.h>

#include <vector>

#include "media/base/audio_decoder_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct MEDIA_EXPORT DemuxerConfigs {
  DemuxerConfigs();
  DemuxerConfigs(const DemuxerConfigs& other);
  ~DemuxerConfigs();

  AudioCodec audio_codec;
  int audio_channels;
  int audio_sampling_rate;
  bool is_audio_encrypted;
  std::vector<uint8_t> audio_extra_data;
  int64_t audio_codec_delay_ns;
  int64_t audio_seek_preroll_ns;

  VideoCodec video_codec;
  gfx::Size video_size;
  bool is_video_encrypted;
  std::vector<uint8_t> video_extra_data;

  base::TimeDelta duration;
};

struct MEDIA_EXPORT AccessUnit {
  AccessUnit();
  AccessUnit(const AccessUnit& other);
  ~AccessUnit();

  DemuxerStream::Status status;
  bool is_end_of_stream;
  // TODO(ycheo): Use the shared memory to transfer the block data.
  std::vector<uint8_t> data;
  base::TimeDelta timestamp;
  std::vector<char> key_id;
  std::vector<char> iv;
  std::vector<media::SubsampleEntry> subsamples;
  bool is_key_frame;
};

struct MEDIA_EXPORT DemuxerData {
  DemuxerData();
  DemuxerData(const DemuxerData& other);
  ~DemuxerData();

  DemuxerStream::Type type;
  std::vector<AccessUnit> access_units;
  // If the last entry in |access_units| has a status equal to |kConfigChanged|,
  // a corresponding DemuxerConfigs is added into this vector. The
  // DemuxerConfigs should only contain information of the stream that is
  // specified by |type|. This solves the issue that we need multiple IPCs when
  // demuxer configs change.
  std::vector<DemuxerConfigs> demuxer_configs;
};

};  // namespace media

// For logging
MEDIA_EXPORT
std::ostream& operator<<(std::ostream& os, media::DemuxerStream::Type type);

MEDIA_EXPORT
std::ostream& operator<<(std::ostream& os, const media::AccessUnit& au);

MEDIA_EXPORT
std::ostream& operator<<(std::ostream& os, const media::DemuxerConfigs& conf);

#endif  // MEDIA_BASE_ANDROID_DEMUXER_STREAM_PLAYER_PARAMS_H_
