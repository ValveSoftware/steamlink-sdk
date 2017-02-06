// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_stream_traits.h"

#include "base/logging.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"

namespace media {

std::string DecoderStreamTraits<DemuxerStream::AUDIO>::ToString() {
  return "audio";
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::InitializeDecoder(
    DecoderType* decoder,
    DemuxerStream* stream,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb) {
  DCHECK(stream->audio_decoder_config().IsValidConfig());
  decoder->Initialize(stream->audio_decoder_config(), cdm_context, init_cb,
                      output_cb);
}

bool DecoderStreamTraits<DemuxerStream::AUDIO>::NeedsBitstreamConversion(
    DecoderType* decoder) {
  return decoder->NeedsBitstreamConversion();
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  PipelineStatistics statistics;
  statistics.audio_bytes_decoded = bytes_decoded;
  statistics_cb.Run(statistics);
}

scoped_refptr<DecoderStreamTraits<DemuxerStream::AUDIO>::OutputType>
    DecoderStreamTraits<DemuxerStream::AUDIO>::CreateEOSOutput() {
  return OutputType::CreateEOSBuffer();
}

std::string DecoderStreamTraits<DemuxerStream::VIDEO>::ToString() {
  return "video";
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::InitializeDecoder(
    DecoderType* decoder,
    DemuxerStream* stream,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb) {
  DCHECK(stream->video_decoder_config().IsValidConfig());
  decoder->Initialize(stream->video_decoder_config(),
                      stream->liveness() == DemuxerStream::LIVENESS_LIVE,
                      cdm_context, init_cb, output_cb);
}

bool DecoderStreamTraits<DemuxerStream::VIDEO>::NeedsBitstreamConversion(
    DecoderType* decoder) {
  return decoder->NeedsBitstreamConversion();
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  PipelineStatistics statistics;
  statistics.video_bytes_decoded = bytes_decoded;
  statistics_cb.Run(statistics);
}

scoped_refptr<DecoderStreamTraits<DemuxerStream::VIDEO>::OutputType>
    DecoderStreamTraits<DemuxerStream::VIDEO>::CreateEOSOutput() {
  return OutputType::CreateEOSFrame();
}

DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderStreamTraits(
    const scoped_refptr<MediaLog>& media_log)
    : media_log_(media_log) {}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnStreamReset(
    DemuxerStream* stream) {
  DCHECK(stream);
  // Stream is likely being seeked to a new timestamp, so make new validator to
  // build new timestamp expectations.
  audio_ts_validator_.reset(
      new AudioTimestampValidator(stream->audio_decoder_config(), media_log_));
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecode(
    const scoped_refptr<DecoderBuffer>& buffer) {
  audio_ts_validator_->CheckForTimestampGap(buffer);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecodeDone(
    const scoped_refptr<OutputType>& buffer) {
  audio_ts_validator_->RecordOutputDuration(buffer);
}

template class DecoderStreamTraits<DemuxerStream::VIDEO>;
template class DecoderStreamTraits<DemuxerStream::AUDIO>;

}  // namespace media
