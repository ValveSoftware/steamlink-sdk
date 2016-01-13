// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoUtilTest : public testing::Test {
 public:
  VideoUtilTest()
      : height_(0),
        y_stride_(0),
        u_stride_(0),
        v_stride_(0) {
  }

  virtual ~VideoUtilTest() {}

  void CreateSourceFrame(int width, int height,
                         int y_stride, int u_stride, int v_stride) {
    EXPECT_GE(y_stride, width);
    EXPECT_GE(u_stride, width / 2);
    EXPECT_GE(v_stride, width / 2);

    height_ = height;
    y_stride_ = y_stride;
    u_stride_ = u_stride;
    v_stride_ = v_stride;

    y_plane_.reset(new uint8[y_stride * height]);
    u_plane_.reset(new uint8[u_stride * height / 2]);
    v_plane_.reset(new uint8[v_stride * height / 2]);
  }

  void CreateDestinationFrame(int width, int height) {
    gfx::Size size(width, height);
    destination_frame_ =
        VideoFrame::CreateFrame(VideoFrame::YV12, size, gfx::Rect(size), size,
                                base::TimeDelta());
  }

  void CopyPlanes() {
    CopyYPlane(y_plane_.get(), y_stride_, height_, destination_frame_.get());
    CopyUPlane(
        u_plane_.get(), u_stride_, height_ / 2, destination_frame_.get());
    CopyVPlane(
        v_plane_.get(), v_stride_, height_ / 2, destination_frame_.get());
  }

 private:
  scoped_ptr<uint8[]> y_plane_;
  scoped_ptr<uint8[]> u_plane_;
  scoped_ptr<uint8[]> v_plane_;

  int height_;
  int y_stride_;
  int u_stride_;
  int v_stride_;

  scoped_refptr<VideoFrame> destination_frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoUtilTest);
};

TEST_F(VideoUtilTest, CopyPlane_Exact) {
  CreateSourceFrame(16, 16, 16, 8, 8);
  CreateDestinationFrame(16, 16);
  CopyPlanes();
}

TEST_F(VideoUtilTest, CopyPlane_SmallerSource) {
  CreateSourceFrame(8, 8, 8, 4, 4);
  CreateDestinationFrame(16, 16);
  CopyPlanes();
}

TEST_F(VideoUtilTest, CopyPlane_SmallerDestination) {
  CreateSourceFrame(16, 16, 16, 8, 8);
  CreateDestinationFrame(8, 8);
  CopyPlanes();
}

namespace {

uint8 src6x4[] = {
  0,  1,  2,  3,  4,  5,
  6,  7,  8,  9, 10, 11,
 12, 13, 14, 15, 16, 17,
 18, 19, 20, 21, 22, 23
};

// Target images, name pattern target_rotation_flipV_flipH.
uint8* target6x4_0_n_n = src6x4;

uint8 target6x4_0_n_y[] = {
  5,  4,  3,  2,  1,  0,
 11, 10,  9,  8,  7,  6,
 17, 16, 15, 14, 13, 12,
 23, 22, 21, 20, 19, 18
};

uint8 target6x4_0_y_n[] = {
 18, 19, 20, 21, 22, 23,
 12, 13, 14, 15, 16, 17,
  6,  7,  8,  9, 10, 11,
  0,  1,  2,  3,  4,  5
};

uint8 target6x4_0_y_y[] = {
 23, 22, 21, 20, 19, 18,
 17, 16, 15, 14, 13, 12,
 11, 10,  9,  8,  7,  6,
  5,  4,  3,  2,  1,  0
};

uint8 target6x4_90_n_n[] = {
 255, 19, 13,  7,  1, 255,
 255, 20, 14,  8,  2, 255,
 255, 21, 15,  9,  3, 255,
 255, 22, 16, 10,  4, 255
};

uint8 target6x4_90_n_y[] = {
 255,  1,  7, 13, 19, 255,
 255,  2,  8, 14, 20, 255,
 255,  3,  9, 15, 21, 255,
 255,  4, 10, 16, 22, 255
};

uint8 target6x4_90_y_n[] = {
 255, 22, 16, 10,  4, 255,
 255, 21, 15,  9,  3, 255,
 255, 20, 14,  8,  2, 255,
 255, 19, 13,  7,  1, 255
};

uint8 target6x4_90_y_y[] = {
 255,  4, 10, 16, 22, 255,
 255,  3,  9, 15, 21, 255,
 255,  2,  8, 14, 20, 255,
 255,  1,  7, 13, 19, 255
};

uint8* target6x4_180_n_n = target6x4_0_y_y;
uint8* target6x4_180_n_y = target6x4_0_y_n;
uint8* target6x4_180_y_n = target6x4_0_n_y;
uint8* target6x4_180_y_y = target6x4_0_n_n;

uint8* target6x4_270_n_n = target6x4_90_y_y;
uint8* target6x4_270_n_y = target6x4_90_y_n;
uint8* target6x4_270_y_n = target6x4_90_n_y;
uint8* target6x4_270_y_y = target6x4_90_n_n;

uint8 src4x6[] = {
  0,  1,  2,  3,
  4,  5,  6,  7,
  8,  9, 10, 11,
 12, 13, 14, 15,
 16, 17, 18, 19,
 20, 21, 22, 23
};

uint8* target4x6_0_n_n = src4x6;

uint8 target4x6_0_n_y[] = {
  3,  2,  1,  0,
  7,  6,  5,  4,
 11, 10,  9,  8,
 15, 14, 13, 12,
 19, 18, 17, 16,
 23, 22, 21, 20
};

uint8 target4x6_0_y_n[] = {
 20, 21, 22, 23,
 16, 17, 18, 19,
 12, 13, 14, 15,
  8,  9, 10, 11,
  4,  5,  6,  7,
  0,  1,  2,  3
};

uint8 target4x6_0_y_y[] = {
 23, 22, 21, 20,
 19, 18, 17, 16,
 15, 14, 13, 12,
 11, 10,  9,  8,
  7,  6,  5,  4,
  3,  2,  1,  0
};

uint8 target4x6_90_n_n[] = {
 255, 255, 255, 255,
  16,  12,   8,   4,
  17,  13,   9,   5,
  18,  14,  10,   6,
  19,  15,  11,   7,
 255, 255, 255, 255
};

uint8 target4x6_90_n_y[] = {
 255, 255, 255, 255,
   4,   8,  12,  16,
   5,   9,  13,  17,
   6,  10,  14,  18,
   7,  11,  15,  19,
 255, 255, 255, 255
};

uint8 target4x6_90_y_n[] = {
 255, 255, 255, 255,
  19,  15,  11,   7,
  18,  14,  10,   6,
  17,  13,   9,   5,
  16,  12,   8,   4,
 255, 255, 255, 255
};

uint8 target4x6_90_y_y[] = {
 255, 255, 255, 255,
   7,  11,  15,  19,
   6,  10,  14,  18,
   5,   9,  13,  17,
   4,   8,  12,  16,
 255, 255, 255, 255
};

uint8* target4x6_180_n_n = target4x6_0_y_y;
uint8* target4x6_180_n_y = target4x6_0_y_n;
uint8* target4x6_180_y_n = target4x6_0_n_y;
uint8* target4x6_180_y_y = target4x6_0_n_n;

uint8* target4x6_270_n_n = target4x6_90_y_y;
uint8* target4x6_270_n_y = target4x6_90_y_n;
uint8* target4x6_270_y_n = target4x6_90_n_y;
uint8* target4x6_270_y_y = target4x6_90_n_n;

struct VideoRotationTestData {
  uint8* src;
  uint8* target;
  int width;
  int height;
  int rotation;
  bool flip_vert;
  bool flip_horiz;
};

const VideoRotationTestData kVideoRotationTestData[] = {
  { src6x4, target6x4_0_n_n, 6, 4, 0, false, false },
  { src6x4, target6x4_0_n_y, 6, 4, 0, false, true },
  { src6x4, target6x4_0_y_n, 6, 4, 0, true, false },
  { src6x4, target6x4_0_y_y, 6, 4, 0, true, true },

  { src6x4, target6x4_90_n_n, 6, 4, 90, false, false },
  { src6x4, target6x4_90_n_y, 6, 4, 90, false, true },
  { src6x4, target6x4_90_y_n, 6, 4, 90, true, false },
  { src6x4, target6x4_90_y_y, 6, 4, 90, true, true },

  { src6x4, target6x4_180_n_n, 6, 4, 180, false, false },
  { src6x4, target6x4_180_n_y, 6, 4, 180, false, true },
  { src6x4, target6x4_180_y_n, 6, 4, 180, true, false },
  { src6x4, target6x4_180_y_y, 6, 4, 180, true, true },

  { src6x4, target6x4_270_n_n, 6, 4, 270, false, false },
  { src6x4, target6x4_270_n_y, 6, 4, 270, false, true },
  { src6x4, target6x4_270_y_n, 6, 4, 270, true, false },
  { src6x4, target6x4_270_y_y, 6, 4, 270, true, true },

  { src4x6, target4x6_0_n_n, 4, 6, 0, false, false },
  { src4x6, target4x6_0_n_y, 4, 6, 0, false, true },
  { src4x6, target4x6_0_y_n, 4, 6, 0, true, false },
  { src4x6, target4x6_0_y_y, 4, 6, 0, true, true },

  { src4x6, target4x6_90_n_n, 4, 6, 90, false, false },
  { src4x6, target4x6_90_n_y, 4, 6, 90, false, true },
  { src4x6, target4x6_90_y_n, 4, 6, 90, true, false },
  { src4x6, target4x6_90_y_y, 4, 6, 90, true, true },

  { src4x6, target4x6_180_n_n, 4, 6, 180, false, false },
  { src4x6, target4x6_180_n_y, 4, 6, 180, false, true },
  { src4x6, target4x6_180_y_n, 4, 6, 180, true, false },
  { src4x6, target4x6_180_y_y, 4, 6, 180, true, true },

  { src4x6, target4x6_270_n_n, 4, 6, 270, false, false },
  { src4x6, target4x6_270_n_y, 4, 6, 270, false, true },
  { src4x6, target4x6_270_y_n, 4, 6, 270, true, false },
  { src4x6, target4x6_270_y_y, 4, 6, 270, true, true }
};

}  // namespace

class VideoUtilRotationTest
    : public testing::TestWithParam<VideoRotationTestData> {
 public:
  VideoUtilRotationTest() {
    dest_.reset(new uint8[GetParam().width * GetParam().height]);
  }

  virtual ~VideoUtilRotationTest() {}

  uint8* dest_plane() { return dest_.get(); }

 private:
  scoped_ptr<uint8[]> dest_;

  DISALLOW_COPY_AND_ASSIGN(VideoUtilRotationTest);
};

TEST_P(VideoUtilRotationTest, Rotate) {
  int rotation = GetParam().rotation;
  EXPECT_TRUE((rotation >= 0) && (rotation < 360) && (rotation % 90 == 0));

  int size = GetParam().width * GetParam().height;
  uint8* dest = dest_plane();
  memset(dest, 255, size);

  RotatePlaneByPixels(GetParam().src, dest, GetParam().width,
                      GetParam().height, rotation,
                      GetParam().flip_vert, GetParam().flip_horiz);

  EXPECT_EQ(memcmp(dest, GetParam().target, size), 0);
}

INSTANTIATE_TEST_CASE_P(, VideoUtilRotationTest,
                        testing::ValuesIn(kVideoRotationTestData));

TEST_F(VideoUtilTest, ComputeLetterboxRegion) {
  EXPECT_EQ(gfx::Rect(167, 0, 666, 500),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 1000, 500),
                                   gfx::Size(640, 480)));
  EXPECT_EQ(gfx::Rect(0, 312, 500, 375),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 500, 1000),
                                   gfx::Size(640, 480)));
  EXPECT_EQ(gfx::Rect(56, 0, 888, 500),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 1000, 500),
                                   gfx::Size(1920, 1080)));
  EXPECT_EQ(gfx::Rect(0, 12, 100, 75),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 100, 100),
                                   gfx::Size(400, 300)));
  EXPECT_EQ(gfx::Rect(0, 250000000, 2000000000, 1500000000),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 2000000000, 2000000000),
                                   gfx::Size(40000, 30000)));
  EXPECT_TRUE(ComputeLetterboxRegion(gfx::Rect(0, 0, 2000000000, 2000000000),
                                     gfx::Size(0, 0)).IsEmpty());
}

TEST_F(VideoUtilTest, LetterboxYUV) {
  int width = 40;
  int height = 30;
  gfx::Size size(width, height);
  scoped_refptr<VideoFrame> frame(
      VideoFrame::CreateFrame(VideoFrame::YV12, size, gfx::Rect(size), size,
                              base::TimeDelta()));

  for (int left_margin = 0; left_margin <= 10; left_margin += 10) {
    for (int right_margin = 0; right_margin <= 10; right_margin += 10) {
      for (int top_margin = 0; top_margin <= 10; top_margin += 10) {
        for (int bottom_margin = 0; bottom_margin <= 10; bottom_margin += 10) {
          gfx::Rect view_area(left_margin, top_margin,
                              width - left_margin - right_margin,
                              height - top_margin - bottom_margin);
          FillYUV(frame.get(), 0x1, 0x2, 0x3);
          LetterboxYUV(frame.get(), view_area);
          for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
              bool inside = x >= view_area.x() &&
                  x < view_area.x() + view_area.width() &&
                  y >= view_area.y() &&
                  y < view_area.y() + view_area.height();
              EXPECT_EQ(frame->data(VideoFrame::kYPlane)[
                  y * frame->stride(VideoFrame::kYPlane) + x],
                        inside ? 0x01 : 0x00);
              EXPECT_EQ(frame->data(VideoFrame::kUPlane)[
                  (y / 2) * frame->stride(VideoFrame::kUPlane) + (x / 2)],
                        inside ? 0x02 : 0x80);
              EXPECT_EQ(frame->data(VideoFrame::kVPlane)[
                  (y / 2) * frame->stride(VideoFrame::kVPlane) + (x / 2)],
                        inside ? 0x03 : 0x80);
            }
          }
        }
      }
    }
  }
}

}  // namespace media
