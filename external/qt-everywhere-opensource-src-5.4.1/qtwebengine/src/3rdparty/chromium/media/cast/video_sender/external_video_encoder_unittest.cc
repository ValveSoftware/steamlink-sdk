// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/fake_video_encode_accelerator.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/cast/video_sender/external_video_encoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

namespace {

void CreateVideoEncodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_ptr<VideoEncodeAccelerator> fake_vea,
    const ReceiveVideoEncodeAcceleratorCallback& callback) {
  callback.Run(task_runner, fake_vea.Pass());
}

void CreateSharedMemory(
    size_t size, const ReceiveVideoEncodeMemoryCallback& callback) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAndMapAnonymous(size)) {
    NOTREACHED();
    return;
  }
  callback.Run(shm.Pass());
}

class TestVideoEncoderCallback
    : public base::RefCountedThreadSafe<TestVideoEncoderCallback> {
 public:
  TestVideoEncoderCallback() {}

  void SetExpectedResult(uint32 expected_frame_id,
                         uint32 expected_last_referenced_frame_id,
                         const base::TimeTicks& expected_capture_time) {
    expected_frame_id_ = expected_frame_id;
    expected_last_referenced_frame_id_ = expected_last_referenced_frame_id;
    expected_capture_time_ = expected_capture_time;
  }

  void DeliverEncodedVideoFrame(
      scoped_ptr<transport::EncodedFrame> encoded_frame) {
    if (expected_frame_id_ == expected_last_referenced_frame_id_) {
      EXPECT_EQ(transport::EncodedFrame::KEY, encoded_frame->dependency);
    } else {
      EXPECT_EQ(transport::EncodedFrame::DEPENDENT,
                encoded_frame->dependency);
    }
    EXPECT_EQ(expected_frame_id_, encoded_frame->frame_id);
    EXPECT_EQ(expected_last_referenced_frame_id_,
              encoded_frame->referenced_frame_id);
    EXPECT_EQ(expected_capture_time_, encoded_frame->reference_time);
  }

 protected:
  virtual ~TestVideoEncoderCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestVideoEncoderCallback>;

  bool expected_key_frame_;
  uint32 expected_frame_id_;
  uint32 expected_last_referenced_frame_id_;
  base::TimeTicks expected_capture_time_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoEncoderCallback);
};
}  // namespace

class ExternalVideoEncoderTest : public ::testing::Test {
 protected:
  ExternalVideoEncoderTest()
      : test_video_encoder_callback_(new TestVideoEncoderCallback()) {
    video_config_.rtp_config.ssrc = 1;
    video_config_.incoming_feedback_ssrc = 2;
    video_config_.rtp_config.payload_type = 127;
    video_config_.use_external_encoder = true;
    video_config_.width = 320;
    video_config_.height = 240;
    video_config_.max_bitrate = 5000000;
    video_config_.min_bitrate = 1000000;
    video_config_.start_bitrate = 2000000;
    video_config_.max_qp = 56;
    video_config_.min_qp = 0;
    video_config_.max_frame_rate = 30;
    video_config_.max_number_of_video_buffers_used = 3;
    video_config_.codec = transport::kVp8;
    gfx::Size size(video_config_.width, video_config_.height);
    video_frame_ = media::VideoFrame::CreateFrame(
        VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(video_frame_, 123);

    testing_clock_ = new base::SimpleTestTickClock();
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);

    fake_vea_ = new test::FakeVideoEncodeAccelerator(task_runner_);
    scoped_ptr<VideoEncodeAccelerator> fake_vea(fake_vea_);
    video_encoder_.reset(
        new ExternalVideoEncoder(cast_environment_,
                                 video_config_,
                                 base::Bind(&CreateVideoEncodeAccelerator,
                                            task_runner_,
                                            base::Passed(&fake_vea)),
                                 base::Bind(&CreateSharedMemory)));
  }

  virtual ~ExternalVideoEncoderTest() {}

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  test::FakeVideoEncodeAccelerator* fake_vea_;  // Owned by video_encoder_.
  scoped_refptr<TestVideoEncoderCallback> test_video_encoder_callback_;
  VideoSenderConfig video_config_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<VideoEncoder> video_encoder_;
  scoped_refptr<media::VideoFrame> video_frame_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVideoEncoderTest);
};

TEST_F(ExternalVideoEncoderTest, EncodePattern30fpsRunningOutOfAck) {
  task_runner_->RunTasks();  // Run the initializer on the correct thread.

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  base::TimeTicks capture_time;
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(0, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, capture_time, frame_encoded_callback));
  task_runner_->RunTasks();

  for (int i = 0; i < 6; ++i) {
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(i + 1, i, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
        video_frame_, capture_time, frame_encoded_callback));
    task_runner_->RunTasks();
  }
  // We need to run the task to cleanup the GPU instance.
  video_encoder_.reset(NULL);
  task_runner_->RunTasks();
}

TEST_F(ExternalVideoEncoderTest, StreamHeader) {
  task_runner_->RunTasks();  // Run the initializer on the correct thread.

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  // Force the FakeVideoEncodeAccelerator to return a dummy non-key frame first.
  fake_vea_->SendDummyFrameForTesting(false);

  // Verify the first returned bitstream buffer is still a key frame.
  base::TimeTicks capture_time;
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(0, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, capture_time, frame_encoded_callback));
  task_runner_->RunTasks();

  // We need to run the task to cleanup the GPU instance.
  video_encoder_.reset(NULL);
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media
