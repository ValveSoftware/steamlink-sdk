// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "media/base/gmock_callback_support.h"
#include "media/filters/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace content {

// TODO(wuchengli): add MockSharedMemroy so more functions can be tested.
class RTCVideoDecoderTest : public ::testing::Test,
                            webrtc::DecodedImageCallback {
 public:
  RTCVideoDecoderTest()
      : mock_gpu_factories_(new media::MockGpuVideoAcceleratorFactories),
        vda_thread_("vda_thread"),
        idle_waiter_(false, false) {
    memset(&codec_, 0, sizeof(codec_));
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(vda_thread_.Start());
    vda_task_runner_ = vda_thread_.message_loop_proxy();
    mock_vda_ = new media::MockVideoDecodeAccelerator;
    EXPECT_CALL(*mock_gpu_factories_, GetTaskRunner())
        .WillRepeatedly(Return(vda_task_runner_));
    EXPECT_CALL(*mock_gpu_factories_, DoCreateVideoDecodeAccelerator())
        .WillRepeatedly(Return(mock_vda_));
    EXPECT_CALL(*mock_gpu_factories_, CreateSharedMemory(_))
        .WillRepeatedly(Return(static_cast<base::SharedMemory*>(NULL)));
    EXPECT_CALL(*mock_vda_, Initialize(_, _))
        .Times(1)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_vda_, Destroy()).Times(1);
    rtc_decoder_ =
        RTCVideoDecoder::Create(webrtc::kVideoCodecVP8, mock_gpu_factories_);
  }

  virtual void TearDown() OVERRIDE {
    VLOG(2) << "TearDown";
    EXPECT_TRUE(vda_thread_.IsRunning());
    RunUntilIdle();  // Wait until all callbascks complete.
    vda_task_runner_->DeleteSoon(FROM_HERE, rtc_decoder_.release());
    // Make sure the decoder is released before stopping the thread.
    RunUntilIdle();
    vda_thread_.Stop();
  }

  virtual int32_t Decoded(webrtc::I420VideoFrame& decoded_image) OVERRIDE {
    VLOG(2) << "Decoded";
    EXPECT_EQ(vda_task_runner_, base::MessageLoopProxy::current());
    return WEBRTC_VIDEO_CODEC_OK;
  }

  void Initialize() {
    VLOG(2) << "Initialize";
    codec_.codecType = webrtc::kVideoCodecVP8;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->InitDecode(&codec_, 1));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_decoder_->RegisterDecodeCompleteCallback(this));
  }

  void NotifyResetDone() {
    VLOG(2) << "NotifyResetDone";
    vda_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::NotifyResetDone,
                   base::Unretained(rtc_decoder_.get())));
  }

  void RunUntilIdle() {
    VLOG(2) << "RunUntilIdle";
    vda_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&base::WaitableEvent::Signal,
                                          base::Unretained(&idle_waiter_)));
    idle_waiter_.Wait();
  }

 protected:
  scoped_refptr<media::MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  media::MockVideoDecodeAccelerator* mock_vda_;
  scoped_ptr<RTCVideoDecoder> rtc_decoder_;
  webrtc::VideoCodec codec_;
  base::Thread vda_thread_;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> vda_task_runner_;

  base::Lock lock_;
  base::WaitableEvent idle_waiter_;
};

TEST_F(RTCVideoDecoderTest, CreateReturnsNullOnUnsupportedCodec) {
  scoped_ptr<RTCVideoDecoder> null_rtc_decoder(
      RTCVideoDecoder::Create(webrtc::kVideoCodecI420, mock_gpu_factories_));
  EXPECT_EQ(NULL, null_rtc_decoder.get());
}

TEST_F(RTCVideoDecoderTest, InitDecodeReturnsErrorOnFeedbackMode) {
  codec_.codecType = webrtc::kVideoCodecVP8;
  codec_.codecSpecific.VP8.feedbackModeOn = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR, rtc_decoder_->InitDecode(&codec_, 1));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorWithoutInitDecode) {
  webrtc::EncodedImage input_image;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_UNINITIALIZED,
            rtc_decoder_->Decode(input_image, false, NULL, NULL, 0));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorOnIncompleteFrame) {
  Initialize();
  webrtc::EncodedImage input_image;
  input_image._completeFrame = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, false, NULL, NULL, 0));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorOnMissingFrames) {
  Initialize();
  webrtc::EncodedImage input_image;
  input_image._completeFrame = true;
  bool missingFrames = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, missingFrames, NULL, NULL, 0));
}

TEST_F(RTCVideoDecoderTest, ResetReturnsOk) {
  Initialize();
  EXPECT_CALL(*mock_vda_, Reset())
      .WillOnce(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Reset());
}

TEST_F(RTCVideoDecoderTest, ReleaseReturnsOk) {
  Initialize();
  EXPECT_CALL(*mock_vda_, Reset())
      .WillOnce(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
}

TEST_F(RTCVideoDecoderTest, InitDecodeAfterRelease) {
  EXPECT_CALL(*mock_vda_, Reset())
      .WillRepeatedly(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  Initialize();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
  Initialize();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
}

TEST_F(RTCVideoDecoderTest, IsBufferAfterReset) {
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

}  // content
