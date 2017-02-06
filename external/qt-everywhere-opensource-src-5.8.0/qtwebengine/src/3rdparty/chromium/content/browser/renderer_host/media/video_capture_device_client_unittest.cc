// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_device_client.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/limits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::InSequence;
using ::testing::SaveArg;

namespace content {

namespace {

class MockVideoCaptureController : public VideoCaptureController {
 public:
  explicit MockVideoCaptureController(int max_buffers)
      : VideoCaptureController(max_buffers) {}
  ~MockVideoCaptureController() override {}

  MOCK_METHOD1(MockDoIncomingCapturedVideoFrameOnIOThread,
               void(const gfx::Size&));
  MOCK_METHOD0(DoErrorOnIOThread, void());
  MOCK_METHOD1(DoLogOnIOThread, void(const std::string& message));
  MOCK_METHOD1(DoBufferDestroyedOnIOThread, void(int buffer_id_to_drop));

  void DoIncomingCapturedVideoFrameOnIOThread(
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame) override {
    MockDoIncomingCapturedVideoFrameOnIOThread(frame->coded_size());
  }
};

class VideoCaptureDeviceClientTest : public ::testing::Test {
 public:
  VideoCaptureDeviceClientTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        controller_(new MockVideoCaptureController(1)),
        device_client_(controller_->NewDeviceClient()) {}
  ~VideoCaptureDeviceClientTest() override {}

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;
  const std::unique_ptr<MockVideoCaptureController> controller_;
  const std::unique_ptr<media::VideoCaptureDevice::Client> device_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceClientTest);
};

}  // namespace

// A small test for reference and to verify VideoCaptureDeviceClient is
// minimally functional.
TEST_F(VideoCaptureDeviceClientTest, Minimal) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const media::VideoCaptureFormat kFrameFormat(
      gfx::Size(10, 10), 30.0f /*frame_rate*/,
      media::PIXEL_FORMAT_I420,
      media::PIXEL_STORAGE_CPU);
  DCHECK(device_client_.get());
  EXPECT_CALL(*controller_, DoLogOnIOThread(_)).Times(1);
  EXPECT_CALL(*controller_, MockDoIncomingCapturedVideoFrameOnIOThread(_))
      .Times(1);
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(controller_.get());
}

// Tests that we don't try to pass on frames with an invalid frame format.
TEST_F(VideoCaptureDeviceClientTest, FailsSilentlyGivenInvalidFrameFormat) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  // kFrameFormat is invalid in a number of ways.
  const media::VideoCaptureFormat kFrameFormat(
      gfx::Size(media::limits::kMaxDimension + 1, media::limits::kMaxDimension),
      media::limits::kMaxFramesPerSecond + 1,
      media::VideoPixelFormat::PIXEL_FORMAT_I420,
      media::VideoPixelStorage::PIXEL_STORAGE_CPU);
  DCHECK(device_client_.get());
  // Expect the the call to fail silently inside the VideoCaptureDeviceClient.
  EXPECT_CALL(*controller_, DoLogOnIOThread(_)).Times(1);
  EXPECT_CALL(*controller_, MockDoIncomingCapturedVideoFrameOnIOThread(_))
      .Times(0);
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(controller_.get());
}

// Tests that we fail silently if no available buffers to use.
TEST_F(VideoCaptureDeviceClientTest, DropsFrameIfNoBuffer) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const media::VideoCaptureFormat kFrameFormat(
      gfx::Size(10, 10), 30.0f /*frame_rate*/,
      media::PIXEL_FORMAT_I420,
      media::PIXEL_STORAGE_CPU);
  // We expect the second frame to be silently dropped, so these should
  // only be called once despite the two frames.
  EXPECT_CALL(*controller_, DoLogOnIOThread(_)).Times(1);
  EXPECT_CALL(*controller_, MockDoIncomingCapturedVideoFrameOnIOThread(_))
      .Times(1);
  // Pass two frames. The second will be dropped.
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(controller_.get());
}

// Tests that buffer-based capture API accepts some memory-backed pixel formats.
TEST_F(VideoCaptureDeviceClientTest, DataCaptureGoodPixelFormats) {
  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not accept all pixel formats. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const gfx::Size kCaptureResolution(10, 10);
  ASSERT_GE(kScratchpadSizeInBytes, kCaptureResolution.GetArea() * 4u)
      << "Scratchpad is too small to hold the largest pixel format (ARGB).";

  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      kCaptureResolution, 30.0f, media::PIXEL_FORMAT_UNKNOWN);

  // Only use the VideoPixelFormats that we know supported. Do not add
  // PIXEL_FORMAT_MJPEG since it would need a real JPEG header.
  const media::VideoPixelFormat kSupportedFormats[] = {
    media::PIXEL_FORMAT_I420,
    media::PIXEL_FORMAT_YV12,
    media::PIXEL_FORMAT_NV12,
    media::PIXEL_FORMAT_NV21,
    media::PIXEL_FORMAT_YUY2,
    media::PIXEL_FORMAT_UYVY,
#if defined(OS_WIN) || defined(OS_LINUX)
    media::PIXEL_FORMAT_RGB24,
#endif
    media::PIXEL_FORMAT_RGB32,
    media::PIXEL_FORMAT_ARGB
  };

  for (media::VideoPixelFormat format : kSupportedFormats) {
    params.requested_format.pixel_format = format;

    EXPECT_CALL(*controller_, DoLogOnIOThread(_)).Times(1);
    EXPECT_CALL(*controller_, MockDoIncomingCapturedVideoFrameOnIOThread(_))
        .Times(1);
    device_client_->OnIncomingCapturedData(
        data, params.requested_format.ImageAllocationSize(),
        params.requested_format, 0 /* clockwise_rotation */, base::TimeTicks(),
        base::TimeDelta());
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(controller_.get());
  }
}

// Test that we receive the expected resolution for a given captured frame
// resolution and rotation. Odd resolutions are also cropped.
TEST_F(VideoCaptureDeviceClientTest, CheckRotationsAndCrops) {
  const struct SizeAndRotation {
    gfx::Size input_resolution;
    int rotation;
    gfx::Size output_resolution;
  } kSizeAndRotations[] = {{{6, 4}, 0, {6, 4}},
                           {{6, 4}, 90, {4, 6}},
                           {{6, 4}, 180, {6, 4}},
                           {{6, 4}, 270, {4, 6}},
                           {{7, 4}, 0, {6, 4}},
                           {{7, 4}, 90, {4, 6}},
                           {{7, 4}, 180, {6, 4}},
                           {{7, 4}, 270, {4, 6}}};

  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not resolve rotations or crops. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};

  EXPECT_CALL(*controller_, DoLogOnIOThread(_)).Times(1);

  media::VideoCaptureParams params;
  for (const auto& size_and_rotation : kSizeAndRotations) {
    ASSERT_GE(kScratchpadSizeInBytes,
              size_and_rotation.input_resolution.GetArea() * 4u)
        << "Scratchpad is too small to hold the largest pixel format (ARGB).";
    params.requested_format =
        media::VideoCaptureFormat(size_and_rotation.input_resolution, 30.0f,
                                  media::PIXEL_FORMAT_ARGB);
    gfx::Size coded_size;
    EXPECT_CALL(*controller_, MockDoIncomingCapturedVideoFrameOnIOThread(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&coded_size));
    device_client_->OnIncomingCapturedData(
        data, params.requested_format.ImageAllocationSize(),
        params.requested_format, size_and_rotation.rotation, base::TimeTicks(),
        base::TimeDelta());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(coded_size.width(), size_and_rotation.output_resolution.width());
    EXPECT_EQ(coded_size.height(),
              size_and_rotation.output_resolution.height());

    Mock::VerifyAndClearExpectations(controller_.get());
  }
}

}  // namespace content
