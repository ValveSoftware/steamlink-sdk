// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/receiver/video_decoder.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/cast/video_sender/codecs/vp8/vp8_encoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

const int kWidth = 360;
const int kHeight = 240;
const int kFrameRate = 10;

VideoSenderConfig GetVideoSenderConfigForTest() {
  VideoSenderConfig config;
  config.width = kWidth;
  config.height = kHeight;
  config.max_frame_rate = kFrameRate;
  return config;
}

}  // namespace

class VideoDecoderTest
    : public ::testing::TestWithParam<transport::VideoCodec> {
 public:
  VideoDecoderTest()
      : cast_environment_(new StandaloneCastEnvironment()),
        vp8_encoder_(GetVideoSenderConfigForTest(), 0),
        cond_(&lock_) {
    vp8_encoder_.Initialize();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    video_decoder_.reset(new VideoDecoder(cast_environment_, GetParam()));
    CHECK_EQ(STATUS_VIDEO_INITIALIZED, video_decoder_->InitializationResult());

    next_frame_timestamp_ = base::TimeDelta();
    last_frame_id_ = 0;
    seen_a_decoded_frame_ = false;

    total_video_frames_feed_in_ = 0;
    total_video_frames_decoded_ = 0;
  }

  // Called from the unit test thread to create another EncodedFrame and push it
  // into the decoding pipeline.
  void FeedMoreVideo(int num_dropped_frames) {
    // Prepare a simulated EncodedFrame to feed into the VideoDecoder.

    const gfx::Size frame_size(kWidth, kHeight);
    const scoped_refptr<VideoFrame> video_frame =
        VideoFrame::CreateFrame(VideoFrame::YV12,
                                frame_size,
                                gfx::Rect(frame_size),
                                frame_size,
                                next_frame_timestamp_);
    next_frame_timestamp_ += base::TimeDelta::FromSeconds(1) / kFrameRate;
    PopulateVideoFrame(video_frame, 0);

    // Encode |frame| into |encoded_frame->data|.
    scoped_ptr<transport::EncodedFrame> encoded_frame(
        new transport::EncodedFrame());
    CHECK_EQ(transport::kVp8, GetParam());  // Only support VP8 test currently.
    vp8_encoder_.Encode(video_frame, encoded_frame.get());
    encoded_frame->frame_id = last_frame_id_ + 1 + num_dropped_frames;
    last_frame_id_ = encoded_frame->frame_id;

    {
      base::AutoLock auto_lock(lock_);
      ++total_video_frames_feed_in_;
    }

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&VideoDecoder::DecodeFrame,
                   base::Unretained(video_decoder_.get()),
                   base::Passed(&encoded_frame),
                   base::Bind(&VideoDecoderTest::OnDecodedFrame,
                              base::Unretained(this),
                              video_frame,
                              num_dropped_frames == 0)));
  }

  // Blocks the caller until all video that has been feed in has been decoded.
  void WaitForAllVideoToBeDecoded() {
    DCHECK(!cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
    base::AutoLock auto_lock(lock_);
    while (total_video_frames_decoded_ < total_video_frames_feed_in_)
      cond_.Wait();
    EXPECT_EQ(total_video_frames_feed_in_, total_video_frames_decoded_);
  }

 private:
  // Called by |vp8_decoder_| to deliver each frame of decoded video.
  void OnDecodedFrame(const scoped_refptr<VideoFrame>& expected_video_frame,
                      bool should_be_continuous,
                      const scoped_refptr<VideoFrame>& video_frame,
                      bool is_continuous) {
    DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

    // A NULL |video_frame| indicates a decode error, which we don't expect.
    ASSERT_FALSE(!video_frame);

    // Did the decoder detect whether frames were dropped?
    EXPECT_EQ(should_be_continuous, is_continuous);

    // Does the video data seem to be intact?
    EXPECT_EQ(expected_video_frame->coded_size().width(),
              video_frame->coded_size().width());
    EXPECT_EQ(expected_video_frame->coded_size().height(),
              video_frame->coded_size().height());
    EXPECT_LT(40.0, I420PSNR(expected_video_frame, video_frame));
    // TODO(miu): Once we start using VideoFrame::timestamp_, check that here.

    // Signal the main test thread that more video was decoded.
    base::AutoLock auto_lock(lock_);
    ++total_video_frames_decoded_;
    cond_.Signal();
  }

  const scoped_refptr<StandaloneCastEnvironment> cast_environment_;
  scoped_ptr<VideoDecoder> video_decoder_;
  base::TimeDelta next_frame_timestamp_;
  uint32 last_frame_id_;
  bool seen_a_decoded_frame_;

  Vp8Encoder vp8_encoder_;

  base::Lock lock_;
  base::ConditionVariable cond_;
  int total_video_frames_feed_in_;
  int total_video_frames_decoded_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderTest);
};

TEST_P(VideoDecoderTest, DecodesFrames) {
  const int kNumFrames = 10;
  for (int i = 0; i < kNumFrames; ++i)
    FeedMoreVideo(0);
  WaitForAllVideoToBeDecoded();
}

TEST_P(VideoDecoderTest, RecoversFromDroppedFrames) {
  const int kNumFrames = 100;
  int next_drop_at = 3;
  int next_num_dropped = 1;
  for (int i = 0; i < kNumFrames; ++i) {
    if (i == next_drop_at) {
      const int num_dropped = next_num_dropped++;
      next_drop_at *= 2;
      i += num_dropped;
      FeedMoreVideo(num_dropped);
    } else {
      FeedMoreVideo(0);
    }
  }
  WaitForAllVideoToBeDecoded();
}

INSTANTIATE_TEST_CASE_P(VideoDecoderTestScenarios,
                        VideoDecoderTest,
                        ::testing::Values(transport::kVp8));

}  // namespace cast
}  // namespace media
