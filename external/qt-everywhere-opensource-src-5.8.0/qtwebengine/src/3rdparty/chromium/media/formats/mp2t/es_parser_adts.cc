// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/es_parser_adts.h"

#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bit_reader.h"
#include "media/base/channel_layout.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/common/offset_byte_queue.h"
#include "media/formats/mp2t/mp2t_common.h"
#include "media/formats/mpeg/adts_constants.h"
#include "media/formats/mpeg/adts_header_parser.h"

namespace media {

static int ExtractAdtsFrameSize(const uint8_t* adts_header) {
  return ((static_cast<int>(adts_header[5]) >> 5) |
          (static_cast<int>(adts_header[4]) << 3) |
          ((static_cast<int>(adts_header[3]) & 0x3) << 11));
}

// Return true if buf corresponds to an ADTS syncword.
// |buf| size must be at least 2.
static bool isAdtsSyncWord(const uint8_t* buf) {
  // The first 12 bits must be 1.
  // The layer field (2 bits) must be set to 0.
  return (buf[0] == 0xff) && ((buf[1] & 0xf6) == 0xf0);
}

namespace mp2t {

struct EsParserAdts::AdtsFrame {
  // Pointer to the ES data.
  const uint8_t* data;

  // Frame size;
  int size;

  // Frame offset in the ES queue.
  int64_t queue_offset;
};

bool EsParserAdts::LookForAdtsFrame(AdtsFrame* adts_frame) {
  int es_size;
  const uint8_t* es;
  es_queue_->Peek(&es, &es_size);

  int max_offset = es_size - kADTSHeaderMinSize;
  if (max_offset <= 0)
    return false;

  for (int offset = 0; offset < max_offset; offset++) {
    const uint8_t* cur_buf = &es[offset];
    if (!isAdtsSyncWord(cur_buf))
      continue;

    int frame_size = ExtractAdtsFrameSize(cur_buf);
    if (frame_size < kADTSHeaderMinSize) {
      // Too short to be an ADTS frame.
      continue;
    }

    int remaining_size = es_size - offset;
    if (remaining_size < frame_size) {
      // Not a full frame: will resume when we have more data.
      es_queue_->Pop(offset);
      return false;
    }

    // Check whether there is another frame
    // |size| apart from the current one.
    if (remaining_size >= frame_size + 2 &&
        !isAdtsSyncWord(&cur_buf[frame_size])) {
      continue;
    }

    es_queue_->Pop(offset);
    es_queue_->Peek(&adts_frame->data, &es_size);
    adts_frame->queue_offset = es_queue_->head();
    adts_frame->size = frame_size;
    DVLOG(LOG_LEVEL_ES)
        << "ADTS syncword @ pos=" << adts_frame->queue_offset
        << " frame_size=" << adts_frame->size;
    DVLOG(LOG_LEVEL_ES)
        << "ADTS header: "
        << base::HexEncode(adts_frame->data, kADTSHeaderMinSize);
    return true;
  }

  es_queue_->Pop(max_offset);
  return false;
}

void EsParserAdts::SkipAdtsFrame(const AdtsFrame& adts_frame) {
  DCHECK_EQ(adts_frame.queue_offset, es_queue_->head());
  es_queue_->Pop(adts_frame.size);
}

EsParserAdts::EsParserAdts(
    const NewAudioConfigCB& new_audio_config_cb,
    const EmitBufferCB& emit_buffer_cb,
    bool sbr_in_mimetype)
  : new_audio_config_cb_(new_audio_config_cb),
    emit_buffer_cb_(emit_buffer_cb),
    sbr_in_mimetype_(sbr_in_mimetype) {
}

EsParserAdts::~EsParserAdts() {
}

bool EsParserAdts::ParseFromEsQueue() {
  // Look for every ADTS frame in the ES buffer.
  AdtsFrame adts_frame;
  while (LookForAdtsFrame(&adts_frame)) {
    // Update the audio configuration if needed.
    DCHECK_GE(adts_frame.size, kADTSHeaderMinSize);
    if (!UpdateAudioConfiguration(adts_frame.data))
      return false;

    // Get the PTS & the duration of this access unit.
    TimingDesc current_timing_desc =
        GetTimingDescriptor(adts_frame.queue_offset);
    if (current_timing_desc.pts != kNoTimestamp())
      audio_timestamp_helper_->SetBaseTimestamp(current_timing_desc.pts);

    if (audio_timestamp_helper_->base_timestamp() == kNoTimestamp()) {
      DVLOG(1) << "Skipping audio frame with unknown timestamp";
      SkipAdtsFrame(adts_frame);
      continue;
    }
    base::TimeDelta current_pts = audio_timestamp_helper_->GetTimestamp();
    base::TimeDelta frame_duration =
        audio_timestamp_helper_->GetFrameDuration(kSamplesPerAACFrame);

    // Emit an audio frame.
    bool is_key_frame = true;

    // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
    // type and allow multiple audio tracks. See https://crbug.com/341581.
    scoped_refptr<StreamParserBuffer> stream_parser_buffer =
        StreamParserBuffer::CopyFrom(
            adts_frame.data,
            adts_frame.size,
            is_key_frame,
            DemuxerStream::AUDIO, 0);
    stream_parser_buffer->set_timestamp(current_pts);
    stream_parser_buffer->SetDecodeTimestamp(
        DecodeTimestamp::FromPresentationTime(current_pts));
    stream_parser_buffer->set_duration(frame_duration);
    emit_buffer_cb_.Run(stream_parser_buffer);

    // Update the PTS of the next frame.
    audio_timestamp_helper_->AddFrames(kSamplesPerAACFrame);

    // Skip the current frame.
    SkipAdtsFrame(adts_frame);
  }

  return true;
}

void EsParserAdts::Flush() {
}

void EsParserAdts::ResetInternal() {
  last_audio_decoder_config_ = AudioDecoderConfig();
}

bool EsParserAdts::UpdateAudioConfiguration(const uint8_t* adts_header) {
  AudioDecoderConfig audio_decoder_config;
  size_t orig_sample_rate = 0;
  if (!ParseAdtsHeader(adts_header, sbr_in_mimetype_, &audio_decoder_config,
                       &orig_sample_rate))
    return false;

  if (!audio_decoder_config.Matches(last_audio_decoder_config_)) {
    DVLOG(1) << "Sampling frequency: "
             << audio_decoder_config.samples_per_second()
             << " SBR=" << sbr_in_mimetype_;
    DVLOG(1) << "Channel layout: "
             << ChannelLayoutToString(audio_decoder_config.channel_layout());

    // For AAC audio with SBR (Spectral Band Replication) the sampling rate is
    // doubled in ParseAdtsHeader above, but AudioTimestampHelper should still
    // use the original sample rate to compute audio timestamps and durations
    // correctly.

    // Reset the timestamp helper to use a new time scale.
    if (audio_timestamp_helper_ &&
        audio_timestamp_helper_->base_timestamp() != kNoTimestamp()) {
      base::TimeDelta base_timestamp = audio_timestamp_helper_->GetTimestamp();
      audio_timestamp_helper_.reset(new AudioTimestampHelper(orig_sample_rate));
      audio_timestamp_helper_->SetBaseTimestamp(base_timestamp);
    } else {
      audio_timestamp_helper_.reset(new AudioTimestampHelper(orig_sample_rate));
    }
    // Audio config notification.
    last_audio_decoder_config_ = audio_decoder_config;
    new_audio_config_cb_.Run(audio_decoder_config);
  }

  return true;
}

}  // namespace mp2t
}  // namespace media
