// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "base/logging.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

MockPipelineClient::MockPipelineClient() {}
MockPipelineClient::~MockPipelineClient() {}

MockPipeline::MockPipeline() {}
MockPipeline::~MockPipeline() {}

void MockPipeline::Start(Demuxer* demuxer,
                         std::unique_ptr<Renderer> renderer,
                         Client* client,
                         const PipelineStatusCB& seek_cb) {
  Start(demuxer, &renderer, client, seek_cb);
}

void MockPipeline::Resume(std::unique_ptr<Renderer> renderer,
                          base::TimeDelta timestamp,
                          const PipelineStatusCB& seek_cb) {
  Resume(&renderer, timestamp, seek_cb);
}

MockDemuxer::MockDemuxer() {}

MockDemuxer::~MockDemuxer() {}

std::string MockDemuxer::GetDisplayName() const {
  return "MockDemuxer";
}

MockDemuxerStream::MockDemuxerStream(DemuxerStream::Type type)
    : type_(type), liveness_(LIVENESS_UNKNOWN) {
}

MockDemuxerStream::~MockDemuxerStream() {}

DemuxerStream::Type MockDemuxerStream::type() const {
  return type_;
}

DemuxerStream::Liveness MockDemuxerStream::liveness() const {
  return liveness_;
}

AudioDecoderConfig MockDemuxerStream::audio_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  return audio_decoder_config_;
}

VideoDecoderConfig MockDemuxerStream::video_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  return video_decoder_config_;
}

void MockDemuxerStream::set_audio_decoder_config(
    const AudioDecoderConfig& config) {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  audio_decoder_config_ = config;
}

void MockDemuxerStream::set_video_decoder_config(
    const VideoDecoderConfig& config) {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  video_decoder_config_ = config;
}

void MockDemuxerStream::set_liveness(DemuxerStream::Liveness liveness) {
  liveness_ = liveness;
}

VideoRotation MockDemuxerStream::video_rotation() {
  return VIDEO_ROTATION_0;
}

std::string MockVideoDecoder::GetDisplayName() const {
  return "MockVideoDecoder";
}

MockVideoDecoder::MockVideoDecoder() {
  ON_CALL(*this, CanReadWithoutStalling()).WillByDefault(Return(true));
}

std::string MockAudioDecoder::GetDisplayName() const {
  return "MockAudioDecoder";
}

MockVideoDecoder::~MockVideoDecoder() {}

MockAudioDecoder::MockAudioDecoder() {}

MockAudioDecoder::~MockAudioDecoder() {}

MockRendererClient::MockRendererClient() {}

MockRendererClient::~MockRendererClient() {}

MockVideoRenderer::MockVideoRenderer() {}

MockVideoRenderer::~MockVideoRenderer() {}

MockAudioRenderer::MockAudioRenderer() {}

MockAudioRenderer::~MockAudioRenderer() {}

MockRenderer::MockRenderer() {}

MockRenderer::~MockRenderer() {}

MockTimeSource::MockTimeSource() {}

MockTimeSource::~MockTimeSource() {}

MockTextTrack::MockTextTrack() {}

MockTextTrack::~MockTextTrack() {}

MockDecryptor::MockDecryptor() {}

MockDecryptor::~MockDecryptor() {}

MockCdmContext::MockCdmContext() {}

MockCdmContext::~MockCdmContext() {}

int MockCdmContext::GetCdmId() const {
  return cdm_id_;
}

void MockCdmContext::set_cdm_id(int cdm_id) {
  cdm_id_ = cdm_id;
}

}  // namespace media
