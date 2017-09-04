// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_FAKE_REMOTING_DEMUXER_STREAM_PROVIDER_H_
#define MEDIA_REMOTING_FAKE_REMOTING_DEMUXER_STREAM_PROVIDER_H_

#include <deque>

#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class DummyDemuxerStream : public DemuxerStream {
 public:
  explicit DummyDemuxerStream(bool is_audio);
  ~DummyDemuxerStream() override;

  // DemuxerStream implementation.
  MOCK_METHOD1(Read, void(const ReadCB& read_cb));
  void FakeRead(const ReadCB& read_cb);
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  Liveness liveness() const override;
  void EnableBitstreamConverter() override {}
  bool SupportsConfigChanges() override;
  VideoRotation video_rotation() override;
  bool enabled() const override;
  void set_enabled(bool enabled, base::TimeDelta timestamp) override {}
  void SetStreamStatusChangeCB(const StreamStatusChangeCB& cb) override {}

  void CreateFakeFrame(size_t size, bool key_frame, int pts_ms);

 private:
  using BufferQueue = std::deque<scoped_refptr<::media::DecoderBuffer>>;
  BufferQueue buffer_queue_;
  ReadCB pending_read_cb_;
  Type type_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  DISALLOW_COPY_AND_ASSIGN(DummyDemuxerStream);
};

// Audio only demuxer stream provider
class FakeRemotingDemuxerStreamProvider : public DemuxerStreamProvider {
 public:
  FakeRemotingDemuxerStreamProvider();
  ~FakeRemotingDemuxerStreamProvider() final;

  // DemuxerStreamProvider implementation.
  DemuxerStream* GetStream(DemuxerStream::Type type) override;

 private:
  std::unique_ptr<DummyDemuxerStream> demuxer_stream_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemotingDemuxerStreamProvider);
};

}  // namespace media

#endif  // MEDIA_REMOTING_FAKE_REMOTING_DEMUXER_STREAM_PROVIDER_H_
