// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WebRtcVideoCapturerAdapterTest
    : public sigslot::has_slots<>,
      public ::testing::Test {
 public:
  WebRtcVideoCapturerAdapterTest()
      : adapter_(false),
        output_frame_width_(0),
        output_frame_height_(0) {
    adapter_.SignalFrameCaptured.connect(
        this, &WebRtcVideoCapturerAdapterTest::OnFrameCaptured);
  }
  ~WebRtcVideoCapturerAdapterTest() override {}

  void TestSourceCropFrame(int capture_width,
                           int capture_height,
                           int cropped_width,
                           int cropped_height,
                           int natural_width,
                           int natural_height) {
    const int horiz_crop = ((capture_width - cropped_width) / 2);
    const int vert_crop = ((capture_height - cropped_height) / 2);

    gfx::Size coded_size(capture_width, capture_height);
    gfx::Size natural_size(natural_width, natural_height);
    gfx::Rect view_rect(horiz_crop, vert_crop, cropped_width, cropped_height);
    scoped_refptr<media::VideoFrame> frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, coded_size, view_rect, natural_size,
        base::TimeDelta());
    adapter_.OnFrameCaptured(frame);
    EXPECT_EQ(natural_width, output_frame_width_);
    EXPECT_EQ(natural_height, output_frame_height_);
  }
 protected:
  void OnFrameCaptured(cricket::VideoCapturer* capturer,
                       const cricket::CapturedFrame* frame) {
    output_frame_width_ = frame->width;
    output_frame_height_ = frame->height;
  }

 private:
  WebRtcVideoCapturerAdapter adapter_;
  int output_frame_width_;
  int output_frame_height_;
};

TEST_F(WebRtcVideoCapturerAdapterTest, CropFrameTo640360) {
  TestSourceCropFrame(640, 480, 640, 360, 640, 360);
}

TEST_F(WebRtcVideoCapturerAdapterTest, CropFrameTo320320) {
  TestSourceCropFrame(640, 480, 480, 480, 320, 320);
}

TEST_F(WebRtcVideoCapturerAdapterTest, Scale720To640360) {
  TestSourceCropFrame(1280, 720, 1280, 720, 640, 360);
}

}  // namespace content
