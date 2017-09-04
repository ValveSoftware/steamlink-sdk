// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "services/video_capture/fake_device_test.h"
#include "services/video_capture/mock_receiver.h"
#include "services/video_capture/public/cpp/capture_settings.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"
#include "services/video_capture/service_test.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace {

struct FrameInfo {
  gfx::Size size;
  media::VideoPixelFormat pixel_format;
  media::VideoFrame::StorageType storage_type;
  bool is_mappable;
  base::TimeDelta timestamp;
};

} // anonymous namespace

namespace video_capture {

TEST_F(FakeDeviceTest, FrameCallbacksArrive) {
  base::RunLoop wait_loop;
  const int kNumFramesToWaitFor = 3;
  int num_frames_arrived = 0;
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::GetProxy(&receiver_proxy));
  EXPECT_CALL(receiver, OnIncomingCapturedVideoFramePtr(_))
      .WillRepeatedly(InvokeWithoutArgs(
          [&wait_loop, &kNumFramesToWaitFor, &num_frames_arrived]() {
            num_frames_arrived += 1;
            if (num_frames_arrived >= kNumFramesToWaitFor) {
              wait_loop.Quit();
            }
          }));

  fake_device_proxy_->Start(requestable_settings_,
                            std::move(receiver_proxy));
  wait_loop.Run();
}

// Tests that frames received from a fake capture device match the requested
// format and have increasing timestamps.
TEST_F(FakeDeviceTest, ReceiveFramesFromFakeCaptureDevice) {
  base::RunLoop wait_loop;
  mojom::ReceiverPtr receiver_proxy;
  constexpr int num_frames_to_receive = 2;
  FrameInfo received_frame_infos[num_frames_to_receive];
  int received_frame_count = 0;
  MockReceiver receiver(mojo::GetProxy(&receiver_proxy));
  EXPECT_CALL(receiver, OnIncomingCapturedVideoFramePtr(_))
      .WillRepeatedly(
          Invoke([&received_frame_infos, &received_frame_count,
                  &num_frames_to_receive, &wait_loop](
              const media::mojom::VideoFramePtr* frame) {
            if (received_frame_count >= num_frames_to_receive)
              return;
            auto video_frame = frame->To<scoped_refptr<media::VideoFrame>>();
            auto& frame_info = received_frame_infos[received_frame_count];
            frame_info.pixel_format = video_frame->format();
            frame_info.storage_type = video_frame->storage_type();
            frame_info.is_mappable = video_frame->IsMappable();
            frame_info.size = video_frame->natural_size();
            frame_info.timestamp = video_frame->timestamp();
            received_frame_count += 1;
            if (received_frame_count == num_frames_to_receive)
              wait_loop.Quit();
          }));

  fake_device_proxy_->Start(
      requestable_settings_, std::move(receiver_proxy));

  wait_loop.Run();

  base::TimeDelta previous_timestamp;
  for (int i = 0; i < num_frames_to_receive; i++) {
    auto& frame_info = received_frame_infos[i];
    // Service is expected to always output I420
    EXPECT_EQ(media::PIXEL_FORMAT_I420, frame_info.pixel_format);
    // Service is expected to always use STORAGE_MOJO_SHARED_BUFFER
    EXPECT_EQ(media::VideoFrame::STORAGE_MOJO_SHARED_BUFFER,
              frame_info.storage_type);
    EXPECT_TRUE(frame_info.is_mappable);
    EXPECT_EQ(requestable_settings_.format.frame_size,
              frame_info.size);
    // Timestamps are expected to increase
    if (i > 0)
      EXPECT_GT(frame_info.timestamp, previous_timestamp);
    previous_timestamp = frame_info.timestamp;
  }
}

}  // namespace video_capture
