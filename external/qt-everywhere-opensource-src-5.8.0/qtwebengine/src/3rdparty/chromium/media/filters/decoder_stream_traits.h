// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
#define MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_

#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/audio_timestamp_validator.h"

namespace media {

class AudioBuffer;
class AudioDecoder;
class CdmContext;
class DecryptingAudioDecoder;
class DecryptingVideoDecoder;
class DemuxerStream;
class VideoDecoder;
class VideoFrame;

template <DemuxerStream::Type StreamType>
class DecoderStreamTraits {};

template <>
class MEDIA_EXPORT DecoderStreamTraits<DemuxerStream::AUDIO> {
 public:
  typedef AudioBuffer OutputType;
  typedef AudioDecoder DecoderType;
  typedef DecryptingAudioDecoder DecryptingDecoderType;
  typedef base::Callback<void(bool success)> InitCB;
  typedef base::Callback<void(const scoped_refptr<OutputType>&)> OutputCB;

  explicit DecoderStreamTraits(const scoped_refptr<MediaLog>& media_log);

  static std::string ToString();
  void InitializeDecoder(DecoderType* decoder,
                         DemuxerStream* stream,
                         CdmContext* cdm_context,
                         const InitCB& init_cb,
                         const OutputCB& output_cb);
  static bool NeedsBitstreamConversion(DecoderType* decoder);
  void OnDecode(const scoped_refptr<DecoderBuffer>& buffer);
  void OnDecodeDone(const scoped_refptr<OutputType>& buffer);
  void OnStreamReset(DemuxerStream* stream);
  static void ReportStatistics(const StatisticsCB& statistics_cb,
                               int bytes_decoded);
  static scoped_refptr<OutputType> CreateEOSOutput();

 private:
  // Validates encoded timestamps match decoded output duration. MEDIA_LOG warns
  // if timestamp gaps are detected. Sufficiently large gaps can lead to AV sync
  // drift.
  std::unique_ptr<AudioTimestampValidator> audio_ts_validator_;

  scoped_refptr<MediaLog> media_log_;
};

template <>
class MEDIA_EXPORT DecoderStreamTraits<DemuxerStream::VIDEO> {
 public:
  typedef VideoFrame OutputType;
  typedef VideoDecoder DecoderType;
  typedef DecryptingVideoDecoder DecryptingDecoderType;
  typedef base::Callback<void(bool success)> InitCB;
  typedef base::Callback<void(const scoped_refptr<OutputType>&)> OutputCB;

  explicit DecoderStreamTraits(const scoped_refptr<MediaLog>& media_log) {}

  static std::string ToString();
  void InitializeDecoder(DecoderType* decoder,
                         DemuxerStream* stream,
                         CdmContext* cdm_context,
                         const InitCB& init_cb,
                         const OutputCB& output_cb);
  static bool NeedsBitstreamConversion(DecoderType* decoder);
  void OnDecode(const scoped_refptr<DecoderBuffer>& buffer) {}
  void OnDecodeDone(const scoped_refptr<OutputType>& buffer) {}
  void OnStreamReset(DemuxerStream* stream) {}
  static void ReportStatistics(const StatisticsCB& statistics_cb,
                               int bytes_decoded);
  static scoped_refptr<OutputType> CreateEOSOutput();
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
