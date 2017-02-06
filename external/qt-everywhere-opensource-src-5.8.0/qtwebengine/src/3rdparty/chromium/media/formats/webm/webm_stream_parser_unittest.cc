// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_stream_parser.h"

#include <memory>

#include "base/bind.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/test_data_util.h"
#include "media/base/text_track_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::SaveArg;
using testing::_;

namespace media {

class WebMStreamParserTest : public testing::Test {
 public:
  WebMStreamParserTest()
      : media_log_(new testing::StrictMock<MockMediaLog>()) {}

 protected:
  void ParseWebMFile(const std::string& filename,
                     const StreamParser::InitParameters& expected_params) {
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    parser_.reset(new WebMStreamParser());
    Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
        base::Bind(&WebMStreamParserTest::OnEncryptedMediaInitData,
                   base::Unretained(this));

    EXPECT_CALL(*this, InitCB(_));
    EXPECT_CALL(*this, NewMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, EndMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, NewBuffersCB(_, _, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(true));
    parser_->Init(
        base::Bind(&WebMStreamParserTest::InitF, base::Unretained(this),
                   expected_params),
        base::Bind(&WebMStreamParserTest::NewConfigCB, base::Unretained(this)),
        base::Bind(&WebMStreamParserTest::NewBuffersCB, base::Unretained(this)),
        false,  // don't ignore_text_track
        encrypted_media_init_data_cb,
        base::Bind(&WebMStreamParserTest::NewMediaSegmentCB,
                   base::Unretained(this)),
        base::Bind(&WebMStreamParserTest::EndMediaSegmentCB,
                   base::Unretained(this)),
        media_log_);
    bool result = parser_->Parse(buffer->data(), buffer->data_size());
    EXPECT_TRUE(result);
  }

  // Verifies only the detected track counts by track type, then chains to the
  // InitCB mock.
  void InitF(const StreamParser::InitParameters& expected_params,
             const StreamParser::InitParameters& params) {
    EXPECT_EQ(expected_params.detected_audio_track_count,
              params.detected_audio_track_count);
    EXPECT_EQ(expected_params.detected_video_track_count,
              params.detected_video_track_count);
    EXPECT_EQ(expected_params.detected_text_track_count,
              params.detected_text_track_count);
    InitCB(params);
  }

  MOCK_METHOD1(InitCB, void(const StreamParser::InitParameters& params));

  bool NewConfigCB(std::unique_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& text_track_map) {
    DCHECK(tracks.get());
    media_tracks_ = std::move(tracks);
    return true;
  }

  MOCK_METHOD3(NewBuffersCB,
               bool(const StreamParser::BufferQueue&,
                    const StreamParser::BufferQueue&,
                    const StreamParser::TextBufferQueueMap&));
  MOCK_METHOD2(OnEncryptedMediaInitData,
               void(EmeInitDataType init_data_type,
                    const std::vector<uint8_t>& init_data));
  MOCK_METHOD0(NewMediaSegmentCB, void());
  MOCK_METHOD0(EndMediaSegmentCB, void());

  scoped_refptr<testing::StrictMock<MockMediaLog>> media_log_;
  std::unique_ptr<WebMStreamParser> parser_;
  std::unique_ptr<MediaTracks> media_tracks_;
};

TEST_F(WebMStreamParserTest, VerifyMediaTrackMetadata) {
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration());
  params.detected_audio_track_count = 1;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear.webm", params);
  EXPECT_NE(media_tracks_.get(), nullptr);

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);

  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.bytestream_track_id(), 1);
  EXPECT_EQ(video_track.kind(), "main");
  EXPECT_EQ(video_track.label(), "");
  EXPECT_EQ(video_track.language(), "und");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track.kind(), "main");
  EXPECT_EQ(audio_track.label(), "");
  EXPECT_EQ(audio_track.language(), "und");
}

TEST_F(WebMStreamParserTest, VerifyDetectedTrack_AudioOnly) {
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration());
  params.detected_audio_track_count = 1;
  params.detected_video_track_count = 0;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear-320x240-audio-only.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);
  EXPECT_EQ(media_tracks_->tracks()[0]->type(), MediaTrack::Audio);
}

TEST_F(WebMStreamParserTest, VerifyDetectedTrack_VideoOnly) {
  StreamParser::InitParameters params(kInfiniteDuration());
  params.detected_audio_track_count = 0;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear-320x240-video-only.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);
  EXPECT_EQ(media_tracks_->tracks()[0]->type(), MediaTrack::Video);
}

TEST_F(WebMStreamParserTest, VerifyDetectedTracks_AVText) {
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration());
  params.detected_audio_track_count = 1;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 1;
  ParseWebMFile("bear-vp8-webvtt.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 2u);
  EXPECT_EQ(media_tracks_->tracks()[0]->type(), MediaTrack::Video);
  EXPECT_EQ(media_tracks_->tracks()[1]->type(), MediaTrack::Audio);
}

}  // namespace media
