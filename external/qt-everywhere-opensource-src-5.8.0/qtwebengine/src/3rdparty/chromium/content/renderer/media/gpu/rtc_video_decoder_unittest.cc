// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/gpu/rtc_video_decoder.h"
#include "media/base/gmock_callback_support.h"
#include "media/renderers/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif  // defined(OS_WIN)

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Values;
using ::testing::WithArgs;

namespace content {

namespace {

static const int kMinResolutionWidth = 16;
static const int kMinResolutionHeight = 16;
static const int kMaxResolutionWidth = 1920;
static const int kMaxResolutionHeight = 1088;

}  // namespace

// TODO(wuchengli): add MockSharedMemory so more functions can be tested.
class RTCVideoDecoderTest
    : public ::testing::TestWithParam<webrtc::VideoCodecType>,
      webrtc::DecodedImageCallback {
 public:
  RTCVideoDecoderTest()
      : mock_gpu_factories_(
            new media::MockGpuVideoAcceleratorFactories(nullptr)),
        vda_thread_("vda_thread"),
        idle_waiter_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {
    memset(&codec_, 0, sizeof(codec_));
  }

  void SetUp() override {
    ASSERT_TRUE(vda_thread_.Start());
    vda_task_runner_ = vda_thread_.task_runner();
    mock_vda_ = new media::MockVideoDecodeAccelerator;

    media::VideoDecodeAccelerator::SupportedProfile supported_profile;
    supported_profile.min_resolution.SetSize(kMinResolutionWidth,
                                             kMinResolutionHeight);
    supported_profile.max_resolution.SetSize(kMaxResolutionWidth,
                                             kMaxResolutionHeight);
    supported_profile.profile = media::H264PROFILE_MAIN;
    capabilities_.supported_profiles.push_back(supported_profile);
    supported_profile.profile = media::VP8PROFILE_ANY;
    capabilities_.supported_profiles.push_back(supported_profile);

    EXPECT_CALL(*mock_gpu_factories_.get(), GetTaskRunner())
        .WillRepeatedly(Return(vda_task_runner_));
    EXPECT_CALL(*mock_gpu_factories_.get(),
                GetVideoDecodeAcceleratorCapabilities())
        .WillRepeatedly(Return(capabilities_));
    EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoDecodeAccelerator())
        .WillRepeatedly(Return(mock_vda_));
    EXPECT_CALL(*mock_vda_, Initialize(_, _))
        .Times(1)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_vda_, Destroy()).Times(1);

#if defined(OS_WIN)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableWin7WebRtcHWH264Decoding);
#endif  // defined(OS_WIN)
  }

  void TearDown() override {
    DVLOG(2) << "TearDown";
    EXPECT_TRUE(vda_thread_.IsRunning());
    RunUntilIdle();  // Wait until all callbascks complete.
    vda_task_runner_->DeleteSoon(FROM_HERE, rtc_decoder_.release());
    // Make sure the decoder is released before stopping the thread.
    RunUntilIdle();
    vda_thread_.Stop();
  }

  int32_t Decoded(webrtc::VideoFrame& decoded_image) override {
    DVLOG(2) << "Decoded";
    EXPECT_EQ(vda_task_runner_, base::ThreadTaskRunnerHandle::Get());
    return WEBRTC_VIDEO_CODEC_OK;
  }

  void CreateDecoder(webrtc::VideoCodecType codec_type) {
    DVLOG(2) << "CreateDecoder";
    codec_.codecType = codec_type;
    rtc_decoder_ =
        RTCVideoDecoder::Create(codec_type, mock_gpu_factories_.get());
  }

  void Initialize() {
    DVLOG(2) << "Initialize";
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->InitDecode(&codec_, 1));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_decoder_->RegisterDecodeCompleteCallback(this));
  }

  void NotifyResetDone() {
    DVLOG(2) << "NotifyResetDone";
    vda_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::NotifyResetDone,
                   base::Unretained(rtc_decoder_.get())));
  }

  void NotifyError(media::VideoDecodeAccelerator::Error error) {
    DVLOG(2) << "NotifyError";
    vda_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::NotifyError,
                   base::Unretained(rtc_decoder_.get()), error));
  }

  void RunUntilIdle() {
    DVLOG(2) << "RunUntilIdle";
    vda_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&base::WaitableEvent::Signal,
                                          base::Unretained(&idle_waiter_)));
    idle_waiter_.Wait();
  }

  void SetUpResetVDA() {
    mock_vda_after_reset_ = new media::MockVideoDecodeAccelerator;
    EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoDecodeAccelerator())
        .WillRepeatedly(Return(mock_vda_after_reset_));
    EXPECT_CALL(*mock_vda_after_reset_, Initialize(_, _))
        .Times(1)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_vda_after_reset_, Destroy()).Times(1);
  }

 protected:
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  media::MockVideoDecodeAccelerator* mock_vda_;
  media::MockVideoDecodeAccelerator* mock_vda_after_reset_;
  std::unique_ptr<RTCVideoDecoder> rtc_decoder_;
  webrtc::VideoCodec codec_;
  base::Thread vda_thread_;
  media::VideoDecodeAccelerator::Capabilities capabilities_;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> vda_task_runner_;

  base::Lock lock_;
  base::WaitableEvent idle_waiter_;
};

TEST_F(RTCVideoDecoderTest, CreateReturnsNullOnUnsupportedCodec) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  std::unique_ptr<RTCVideoDecoder> null_rtc_decoder(RTCVideoDecoder::Create(
      webrtc::kVideoCodecI420, mock_gpu_factories_.get()));
  EXPECT_EQ(NULL, null_rtc_decoder.get());
}

TEST_P(RTCVideoDecoderTest, CreateAndInitSucceeds) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateDecoder(codec_type);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->InitDecode(&codec_, 1));
}

TEST_F(RTCVideoDecoderTest, InitDecodeReturnsErrorOnFeedbackMode) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  codec_.codecSpecific.VP8.feedbackModeOn = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR, rtc_decoder_->InitDecode(&codec_, 1));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorWithoutInitDecode) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  webrtc::EncodedImage input_image;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_UNINITIALIZED,
            rtc_decoder_->Decode(input_image, false, nullptr, nullptr, 0));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorOnIncompleteFrame) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  Initialize();
  webrtc::EncodedImage input_image;
  input_image._completeFrame = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, false, nullptr, nullptr, 0));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorOnMissingFrames) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  Initialize();
  webrtc::EncodedImage input_image;
  input_image._completeFrame = true;
  bool missingFrames = true;
  EXPECT_EQ(
      WEBRTC_VIDEO_CODEC_ERROR,
      rtc_decoder_->Decode(input_image, missingFrames, nullptr, nullptr, 0));
}

TEST_F(RTCVideoDecoderTest, ReleaseReturnsOk) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  Initialize();
  EXPECT_CALL(*mock_vda_, Reset())
      .WillOnce(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
}

TEST_F(RTCVideoDecoderTest, InitDecodeAfterRelease) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  EXPECT_CALL(*mock_vda_, Reset())
      .WillRepeatedly(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  Initialize();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
  Initialize();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
}

TEST_F(RTCVideoDecoderTest, IsBufferAfterReset) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_INVALID));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                               RTCVideoDecoder::ID_INVALID));
  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_HALF - 2,
                                                RTCVideoDecoder::ID_HALF + 2));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_HALF + 2,
                                               RTCVideoDecoder::ID_HALF - 2));

  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(0, 0));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_LAST));
  EXPECT_FALSE(
      rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_HALF - 2));
  EXPECT_TRUE(
      rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_HALF + 2));

  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST, 0));
  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                                RTCVideoDecoder::ID_HALF - 2));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                               RTCVideoDecoder::ID_HALF + 2));
  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                                RTCVideoDecoder::ID_LAST));
}

TEST_F(RTCVideoDecoderTest, IsFirstBufferAfterReset) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  EXPECT_TRUE(
      rtc_decoder_->IsFirstBufferAfterReset(0, RTCVideoDecoder::ID_INVALID));
  EXPECT_FALSE(
      rtc_decoder_->IsFirstBufferAfterReset(1, RTCVideoDecoder::ID_INVALID));
  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(0, 0));
  EXPECT_TRUE(rtc_decoder_->IsFirstBufferAfterReset(1, 0));
  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(2, 0));

  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(RTCVideoDecoder::ID_HALF,
                                                     RTCVideoDecoder::ID_HALF));
  EXPECT_TRUE(rtc_decoder_->IsFirstBufferAfterReset(
      RTCVideoDecoder::ID_HALF + 1, RTCVideoDecoder::ID_HALF));
  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(
      RTCVideoDecoder::ID_HALF + 2, RTCVideoDecoder::ID_HALF));

  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                                     RTCVideoDecoder::ID_LAST));
  EXPECT_TRUE(
      rtc_decoder_->IsFirstBufferAfterReset(0, RTCVideoDecoder::ID_LAST));
  EXPECT_FALSE(
      rtc_decoder_->IsFirstBufferAfterReset(1, RTCVideoDecoder::ID_LAST));
}

// Tests/Verifies that |rtc_encoder_| drops incoming frames and its error
// counter is increased when in an error condition.
TEST_P(RTCVideoDecoderTest, GetVDAErrorCounterForTesting) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateDecoder(codec_type);
  Initialize();

  webrtc::EncodedImage input_image;
  input_image._completeFrame = true;
  input_image._encodedWidth = kMinResolutionWidth;
  input_image._encodedHeight = kMaxResolutionHeight;
  input_image._frameType = webrtc::kVideoFrameDelta;
  input_image._length = kMinResolutionWidth * kMaxResolutionHeight;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, false, nullptr, nullptr, 0));
  RunUntilIdle();

  // Notify the decoder about a platform error.
  NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
  RunUntilIdle();
  EXPECT_EQ(1, rtc_decoder_->GetVDAErrorCounterForTesting());

  // Expect decode call to reset decoder, and set up a new VDA to track it.
  SetUpResetVDA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, false, nullptr, nullptr, 0));
  EXPECT_EQ(1, rtc_decoder_->GetVDAErrorCounterForTesting());

  // Decoder expects a keyframe after reset, so drops any other frames. However,
  // we should still increment the error counter.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, false, nullptr, nullptr, 0));
  EXPECT_EQ(2, rtc_decoder_->GetVDAErrorCounterForTesting());
}

INSTANTIATE_TEST_CASE_P(CodecProfiles,
                        RTCVideoDecoderTest,
                        Values(webrtc::kVideoCodecVP8,
                               webrtc::kVideoCodecH264));

}  // content
