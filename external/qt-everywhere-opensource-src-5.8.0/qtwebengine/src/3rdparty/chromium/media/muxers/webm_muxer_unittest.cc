// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/webm_muxer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Not;
using ::testing::Sequence;
using ::testing::TestWithParam;
using ::testing::ValuesIn;
using ::testing::WithArgs;

namespace media {

struct kTestParams {
  VideoCodec codec;
  size_t num_video_tracks;
  size_t num_audio_tracks;
};

class WebmMuxerTest : public TestWithParam<kTestParams> {
 public:
  WebmMuxerTest()
      : webm_muxer_(
            GetParam().codec,
            GetParam().num_video_tracks,
            GetParam().num_audio_tracks,
            base::Bind(&WebmMuxerTest::WriteCallback, base::Unretained(this))),
        last_encoded_length_(0),
        accumulated_position_(0) {
    EXPECT_EQ(webm_muxer_.Position(), 0);
    const mkvmuxer::int64 kRandomNewPosition = 333;
    EXPECT_EQ(webm_muxer_.Position(kRandomNewPosition), -1);
    EXPECT_FALSE(webm_muxer_.Seekable());
  }

  MOCK_METHOD1(WriteCallback, void(base::StringPiece));

  void SaveEncodedDataLen(const base::StringPiece& encoded_data) {
    last_encoded_length_ = encoded_data.size();
    accumulated_position_ += encoded_data.size();
  }

  mkvmuxer::int64 GetWebmMuxerPosition() const {
    return webm_muxer_.Position();
  }

  mkvmuxer::Segment::Mode GetWebmSegmentMode() const {
    return webm_muxer_.segment_.mode();
  }

  mkvmuxer::int32 WebmMuxerWrite(const void* buf, mkvmuxer::uint32 len) {
    return webm_muxer_.Write(buf, len);
  }

  WebmMuxer webm_muxer_;

  size_t last_encoded_length_;
  int64_t accumulated_position_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebmMuxerTest);
};

// Checks that the WriteCallback is called with appropriate params when
// WebmMuxer::Write() method is called.
TEST_P(WebmMuxerTest, Write) {
  const base::StringPiece encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(encoded_data));
  WebmMuxerWrite(encoded_data.data(), encoded_data.size());

  EXPECT_EQ(GetWebmMuxerPosition(), static_cast<int64_t>(encoded_data.size()));
}

// This test sends two frames and checks that the WriteCallback is called with
// appropriate params in both cases.
TEST_P(WebmMuxerTest, OnEncodedVideoTwoFrames) {
  if (GetParam().num_audio_tracks > 0)
    return;

  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);
  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedVideo(video_frame,
                             base::WrapUnique(new std::string(encoded_data)),
                             base::TimeTicks::Now(), false /* keyframe */);

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedVideo(video_frame,
                             base::WrapUnique(new std::string(encoded_data)),
                             base::TimeTicks::Now(), false /* keyframe */);

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data.size()),
            accumulated_position_);
}

TEST_P(WebmMuxerTest, OnEncodedAudioTwoFrames) {
  if (GetParam().num_video_tracks > 0)
    return;

  const int sample_rate = 48000;
  const int bits_per_sample = 16;
  const int frames_per_buffer = 480;
  media::AudioParameters audio_params(
      media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_MONO, sample_rate, bits_per_sample,
      frames_per_buffer);

  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedAudio(audio_params,
                             base::WrapUnique(new std::string(encoded_data)),
                             base::TimeTicks::Now());

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedAudio(audio_params,
                             base::WrapUnique(new std::string(encoded_data)),
                             base::TimeTicks::Now());

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data.size()),
            accumulated_position_);
}

// This test verifies that when video data comes before audio data, we save the
// encoded video frames and add it to the video track when audio data arrives.
TEST_P(WebmMuxerTest, VideoIsStoredWhileWaitingForAudio) {
  // This test is only relevant if we have both kinds of tracks.
  if (GetParam().num_video_tracks == 0 || GetParam().num_audio_tracks == 0)
    return;

  // First send a video keyframe
  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);
  const std::string encoded_video("thisisanencodedvideopacket");
  webm_muxer_.OnEncodedVideo(video_frame,
                             base::WrapUnique(new std::string(encoded_video)),
                             base::TimeTicks::Now(), true /* keyframe */);
  // A few encoded non key frames.
  const int kNumNonKeyFrames = 2;
  for (int i = 0; i < kNumNonKeyFrames; ++i) {
    webm_muxer_.OnEncodedVideo(video_frame,
                               base::WrapUnique(new std::string(encoded_video)),
                               base::TimeTicks::Now(), false /* keyframe */);
  }

  // Send some audio. The header will be written and muxing will proceed
  // normally.
  const int sample_rate = 48000;
  const int bits_per_sample = 16;
  const int frames_per_buffer = 480;
  media::AudioParameters audio_params(
      media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_MONO, sample_rate, bits_per_sample,
      frames_per_buffer);
  const std::string encoded_audio("thisisanencodedaudiopacket");

  // We should first get the encoded video frames, then the encoded audio frame.
  Sequence s;
  EXPECT_CALL(*this, WriteCallback(Eq(encoded_video)))
      .Times(1 + kNumNonKeyFrames)
      .InSequence(s);
  EXPECT_CALL(*this, WriteCallback(Eq(encoded_audio))).Times(1).InSequence(s);
  // We'll also get lots of other header-related stuff.
  EXPECT_CALL(*this, WriteCallback(
                         AllOf(Not(Eq(encoded_video)), Not(Eq(encoded_audio)))))
      .Times(AnyNumber());
  webm_muxer_.OnEncodedAudio(audio_params,
                             base::WrapUnique(new std::string(encoded_audio)),
                             base::TimeTicks::Now());
}

const kTestParams kTestCases[] = {
    {kCodecVP8, 1 /* num_video_tracks */, 0 /*num_audio_tracks*/},
    {kCodecVP8, 0, 1},
    {kCodecVP8, 1, 1},
    {kCodecVP9, 1, 0},
    {kCodecVP9, 0, 1},
    {kCodecVP9, 1, 1},
    {kCodecH264, 1, 0},
    {kCodecH264, 0, 1},
    {kCodecH264, 1, 1},
};

INSTANTIATE_TEST_CASE_P(, WebmMuxerTest, ValuesIn(kTestCases));

}  // namespace media
