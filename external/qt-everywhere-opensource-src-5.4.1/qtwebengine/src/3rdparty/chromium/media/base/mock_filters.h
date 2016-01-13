// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include <string>

#include "base/callback.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_renderer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer.h"
#include "media/base/filter_collection.h"
#include "media/base/pipeline_status.h"
#include "media/base/text_track.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();
  virtual ~MockDemuxer();

  // Demuxer implementation.
  MOCK_METHOD3(Initialize,
               void(DemuxerHost* host, const PipelineStatusCB& cb, bool));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());
  MOCK_METHOD1(GetStream, DemuxerStream*(DemuxerStream::Type));
  MOCK_CONST_METHOD0(GetStartTime, base::TimeDelta());
  MOCK_CONST_METHOD0(GetTimelineOffset, base::Time());
  MOCK_CONST_METHOD0(GetLiveness, Liveness());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  explicit MockDemuxerStream(DemuxerStream::Type type);
  virtual ~MockDemuxerStream();

  // DemuxerStream implementation.
  virtual Type type() OVERRIDE;
  MOCK_METHOD1(Read, void(const ReadCB& read_cb));
  virtual AudioDecoderConfig audio_decoder_config() OVERRIDE;
  virtual VideoDecoderConfig video_decoder_config() OVERRIDE;
  MOCK_METHOD0(EnableBitstreamConverter, void());
  MOCK_METHOD0(SupportsConfigChanges, bool());

  void set_audio_decoder_config(const AudioDecoderConfig& config);
  void set_video_decoder_config(const VideoDecoderConfig& config);

 private:
  DemuxerStream::Type type_;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder();
  virtual ~MockVideoDecoder();

  // VideoDecoder implementation.
  MOCK_METHOD4(Initialize, void(const VideoDecoderConfig& config,
                                bool low_delay,
                                const PipelineStatusCB& status_cb,
                                const OutputCB& output_cb));
  MOCK_METHOD2(Decode, void(const scoped_refptr<DecoderBuffer>& buffer,
                            const DecodeCB&));
  MOCK_METHOD1(Reset, void(const base::Closure&));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(HasAlpha, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder();
  virtual ~MockAudioDecoder();

  // AudioDecoder implementation.
  MOCK_METHOD3(Initialize,
               void(const AudioDecoderConfig& config,
                    const PipelineStatusCB& status_cb,
                    const OutputCB& output_cb));
  MOCK_METHOD2(Decode,
               void(const scoped_refptr<DecoderBuffer>& buffer,
                    const DecodeCB&));
  MOCK_METHOD1(Reset, void(const base::Closure&));
  MOCK_METHOD0(Stop, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer();
  virtual ~MockVideoRenderer();

  // VideoRenderer implementation.
  MOCK_METHOD9(Initialize, void(DemuxerStream* stream,
                                bool low_delay,
                                const PipelineStatusCB& init_cb,
                                const StatisticsCB& statistics_cb,
                                const TimeCB& time_cb,
                                const base::Closure& ended_cb,
                                const PipelineStatusCB& error_cb,
                                const TimeDeltaCB& get_time_cb,
                                const TimeDeltaCB& get_duration_cb));
  MOCK_METHOD1(Play, void(const base::Closure& callback));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD2(Preroll, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer();
  virtual ~MockAudioRenderer();

  // AudioRenderer implementation.
  MOCK_METHOD7(Initialize, void(DemuxerStream* stream,
                                const PipelineStatusCB& init_cb,
                                const StatisticsCB& statistics_cb,
                                const base::Closure& underflow_cb,
                                const TimeCB& time_cb,
                                const base::Closure& ended_cb,
                                const PipelineStatusCB& error_cb));
  MOCK_METHOD0(StartRendering, void());
  MOCK_METHOD0(StopRendering, void());
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Preroll, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD0(ResumeAfterUnderflow, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

class MockTextTrack : public TextTrack {
 public:
  MockTextTrack();
  virtual ~MockTextTrack();

  MOCK_METHOD5(addWebVTTCue, void(const base::TimeDelta& start,
                                  const base::TimeDelta& end,
                                  const std::string& id,
                                  const std::string& content,
                                  const std::string& settings));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTextTrack);
};

class MockDecryptor : public Decryptor {
 public:
  MockDecryptor();
  virtual ~MockDecryptor();

  MOCK_METHOD2(RegisterNewKeyCB, void(StreamType stream_type,
                                      const NewKeyCB& new_key_cb));
  MOCK_METHOD3(Decrypt, void(StreamType stream_type,
                             const scoped_refptr<DecoderBuffer>& encrypted,
                             const DecryptCB& decrypt_cb));
  MOCK_METHOD1(CancelDecrypt, void(StreamType stream_type));
  MOCK_METHOD2(InitializeAudioDecoder,
               void(const AudioDecoderConfig& config,
                    const DecoderInitCB& init_cb));
  MOCK_METHOD2(InitializeVideoDecoder,
               void(const VideoDecoderConfig& config,
                    const DecoderInitCB& init_cb));
  MOCK_METHOD2(DecryptAndDecodeAudio,
               void(const scoped_refptr<media::DecoderBuffer>& encrypted,
                    const AudioDecodeCB& audio_decode_cb));
  MOCK_METHOD2(DecryptAndDecodeVideo,
               void(const scoped_refptr<media::DecoderBuffer>& encrypted,
                    const VideoDecodeCB& video_decode_cb));
  MOCK_METHOD1(ResetDecoder, void(StreamType stream_type));
  MOCK_METHOD1(DeinitializeDecoder, void(StreamType stream_type));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecryptor);
};

// Helper mock statistics callback.
class MockStatisticsCB {
 public:
  MockStatisticsCB();
  ~MockStatisticsCB();

  MOCK_METHOD1(OnStatistics, void(const media::PipelineStatistics& statistics));
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
