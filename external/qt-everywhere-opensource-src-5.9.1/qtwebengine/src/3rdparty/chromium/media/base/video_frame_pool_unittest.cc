// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "media/base/video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class VideoFramePoolTest : public ::testing::Test {
 public:
  VideoFramePoolTest() : pool_(new VideoFramePool()) {}

  scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format,
                                        int timestamp_ms) {
    gfx::Size coded_size(320,240);
    gfx::Rect visible_rect(coded_size);
    gfx::Size natural_size(coded_size);

    scoped_refptr<VideoFrame> frame =
        pool_->CreateFrame(
            format, coded_size, visible_rect, natural_size,
            base::TimeDelta::FromMilliseconds(timestamp_ms));
    EXPECT_EQ(format, frame->format());
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(timestamp_ms),
              frame->timestamp());
    EXPECT_EQ(coded_size, frame->coded_size());
    EXPECT_EQ(visible_rect, frame->visible_rect());
    EXPECT_EQ(natural_size, frame->natural_size());

    return frame;
  }

  void CheckPoolSize(size_t size) const {
    EXPECT_EQ(size, pool_->GetPoolSizeForTesting());
  }

 protected:
  std::unique_ptr<VideoFramePool> pool_;
};

TEST_F(VideoFramePoolTest, FrameInitializedAndZeroed) {
  scoped_refptr<VideoFrame> frame = CreateFrame(PIXEL_FORMAT_YV12, 10);

  // Verify that frame is initialized with zeros.
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i)
    EXPECT_EQ(0, frame->data(i)[0]);
}

TEST_F(VideoFramePoolTest, SimpleFrameReuse) {
  scoped_refptr<VideoFrame> frame = CreateFrame(PIXEL_FORMAT_YV12, 10);
  const uint8_t* old_y_data = frame->data(VideoFrame::kYPlane);

  // Clear frame reference to return the frame to the pool.
  frame = NULL;

  // Verify that the next frame from the pool uses the same memory.
  scoped_refptr<VideoFrame> new_frame = CreateFrame(PIXEL_FORMAT_YV12, 20);
  EXPECT_EQ(old_y_data, new_frame->data(VideoFrame::kYPlane));
}

TEST_F(VideoFramePoolTest, SimpleFormatChange) {
  scoped_refptr<VideoFrame> frame_a = CreateFrame(PIXEL_FORMAT_YV12, 10);
  scoped_refptr<VideoFrame> frame_b = CreateFrame(PIXEL_FORMAT_YV12, 10);

  // Clear frame references to return the frames to the pool.
  frame_a = NULL;
  frame_b = NULL;

  // Verify that both frames are in the pool.
  CheckPoolSize(2u);

  // Verify that requesting a frame with a different format causes the pool
  // to get drained.
  scoped_refptr<VideoFrame> new_frame = CreateFrame(PIXEL_FORMAT_YV12A, 10);
  CheckPoolSize(0u);
}

TEST_F(VideoFramePoolTest, FrameValidAfterPoolDestruction) {
  scoped_refptr<VideoFrame> frame = CreateFrame(PIXEL_FORMAT_YV12, 10);

  // Destroy the pool.
  pool_.reset();

  // Write to the Y plane. The memory tools should detect a
  // use-after-free if the storage was actually removed by pool destruction.
  memset(frame->data(VideoFrame::kYPlane), 0xff,
         frame->rows(VideoFrame::kYPlane) * frame->stride(VideoFrame::kYPlane));
}

}  // namespace media
