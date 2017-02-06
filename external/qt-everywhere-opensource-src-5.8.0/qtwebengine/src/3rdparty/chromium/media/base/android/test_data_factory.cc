// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/test_data_factory.h"

#include <iterator>

#include "base/strings/stringprintf.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"

namespace media {

DemuxerConfigs TestDataFactory::CreateAudioConfigs(AudioCodec audio_codec,
                                                   base::TimeDelta duration) {
  DemuxerConfigs configs;
  configs.audio_codec = audio_codec;
  configs.audio_channels = 2;
  configs.is_audio_encrypted = false;
  configs.duration = duration;

  switch (audio_codec) {
    case kCodecVorbis: {
      configs.audio_sampling_rate = 44100;
      scoped_refptr<DecoderBuffer> buffer =
          ReadTestDataFile("vorbis-extradata");
      configs.audio_extra_data = std::vector<uint8_t>(
          buffer->data(), buffer->data() + buffer->data_size());
    } break;

    case kCodecAAC: {
      configs.audio_sampling_rate = 44100;
      configs.audio_extra_data = {0x12, 0x10};
    } break;

    default:
      // Other codecs are not supported by this helper.
      NOTREACHED();
      break;
  }

  return configs;
}

DemuxerConfigs TestDataFactory::CreateVideoConfigs(
    VideoCodec video_codec,
    base::TimeDelta duration,
    const gfx::Size& video_size) {
  DemuxerConfigs configs;
  configs.video_codec = video_codec;
  configs.video_size = video_size;
  configs.is_video_encrypted = false;
  configs.duration = duration;

  return configs;
}

TestDataFactory::TestDataFactory(const char* file_name_template,
                                 base::TimeDelta duration,
                                 base::TimeDelta frame_period)
    : duration_(duration),
      frame_period_(frame_period),
      total_chunks_(0),
      starvation_mode_(false),
      eos_reached_(false) {
  LoadPackets(file_name_template);
}

TestDataFactory::~TestDataFactory() {}

bool TestDataFactory::CreateChunk(DemuxerData* chunk, base::TimeDelta* delay) {
  DCHECK(chunk);
  DCHECK(delay);

  if (eos_reached_)
    return false;

  *delay = base::TimeDelta();

  if (!total_chunks_ &&
      HasReconfigForInterval(base::TimeDelta::FromMilliseconds(-1),
                             base::TimeDelta())) {
    // Since the configs AU has to come last in the chunk the initial configs
    // preceeding any other data has to be the only unit in the chunk.
    AddConfiguration(chunk);
    ++total_chunks_;
    return true;
  }

  for (int i = 0; i < 4; ++i) {
    chunk->access_units.push_back(AccessUnit());
    AccessUnit& unit = chunk->access_units.back();

    unit.status = DemuxerStream::kOk;
    unit.timestamp = regular_pts_;
    unit.data = packet_[i];

    regular_pts_ += frame_period_;
  }

  if (chunk->access_units.back().timestamp > duration_) {
    eos_reached_ = true;

    // Replace last access unit with stand-alone EOS if we exceeded duration.
    if (!starvation_mode_) {
      AccessUnit& unit = chunk->access_units.back();
      unit.is_end_of_stream = true;
      unit.data.clear();
    }
  }

  // Allow for modification by subclasses.
  ModifyChunk(chunk);

  // Maintain last PTS.
  for (const AccessUnit& unit : chunk->access_units) {
    if (last_pts_ < unit.timestamp && !unit.data.empty())
      last_pts_ = unit.timestamp;
  }

  // Replace last access unit with |kConfigChanged| if we have a config
  // request for the chunk's interval.
  base::TimeDelta new_chunk_begin_pts = regular_pts_;

  // The interval is [first, last)
  if (HasReconfigForInterval(chunk_begin_pts_, new_chunk_begin_pts)) {
    eos_reached_ = false;
    regular_pts_ -= frame_period_;
    chunk->access_units.pop_back();
    AddConfiguration(chunk);
  }
  chunk_begin_pts_ = new_chunk_begin_pts;

  ++total_chunks_;
  return true;
}

void TestDataFactory::SeekTo(const base::TimeDelta& seek_time) {
  regular_pts_ = seek_time;
  chunk_begin_pts_ = seek_time;
  last_pts_ = base::TimeDelta();
  eos_reached_ = false;
}

void TestDataFactory::RequestInitialConfigs() {
  reconfigs_.insert(base::TimeDelta::FromMilliseconds(-1));
}

void TestDataFactory::RequestConfigChange(base::TimeDelta config_position) {
  reconfigs_.insert(config_position);
}

void TestDataFactory::LoadPackets(const char* file_name_template) {
  for (int i = 0; i < 4; ++i) {
    scoped_refptr<DecoderBuffer> buffer =
        ReadTestDataFile(base::StringPrintf(file_name_template, i));
    packet_[i] = std::vector<uint8_t>(buffer->data(),
                                      buffer->data() + buffer->data_size());
  }
}

bool TestDataFactory::HasReconfigForInterval(base::TimeDelta left,
                                             base::TimeDelta right) const {
  // |first| points to an element greater or equal to |left|.
  PTSSet::const_iterator first = reconfigs_.lower_bound(left);

  // |last| points to an element greater or equal to |right|.
  PTSSet::const_iterator last = reconfigs_.lower_bound(right);

  return std::distance(first, last);
}

void TestDataFactory::AddConfiguration(DemuxerData* chunk) {
  DCHECK(chunk);
  chunk->access_units.push_back(AccessUnit());
  AccessUnit& unit = chunk->access_units.back();
  unit.status = DemuxerStream::kConfigChanged;

  DCHECK(chunk->demuxer_configs.empty());
  chunk->demuxer_configs.push_back(GetConfigs());
}

}  // namespace media
