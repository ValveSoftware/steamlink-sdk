// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "media/filters/skcanvas_video_renderer.h"

using media::VideoFrame;

namespace media {

static const int kWidth = 320;
static const int kHeight = 240;
static const gfx::Rect kNaturalRect(0, 0, kWidth, kHeight);

// Helper for filling a |canvas| with a solid |color|.
void FillCanvas(SkCanvas* canvas, SkColor color) {
  canvas->clear(color);
}

// Helper for returning the color of a solid |canvas|.
SkColor GetColorAt(SkCanvas* canvas, int x, int y) {
  SkBitmap bitmap;
  if (!bitmap.allocN32Pixels(1, 1))
    return 0;
  if (!canvas->readPixels(&bitmap, x, y))
    return 0;
  return bitmap.getColor(0, 0);
}

SkColor GetColor(SkCanvas* canvas) {
  return GetColorAt(canvas, 0, 0);
}

class SkCanvasVideoRendererTest : public testing::Test {
 public:
  enum Color {
    kNone,
    kRed,
    kGreen,
    kBlue,
  };

  SkCanvasVideoRendererTest();
  virtual ~SkCanvasVideoRendererTest();

  // Paints to |canvas| using |renderer_| without any frame data.
  void PaintWithoutFrame(SkCanvas* canvas);

  // Paints the |video_frame| to the |canvas| using |renderer_|, setting the
  // color of |video_frame| to |color| first.
  void Paint(VideoFrame* video_frame, SkCanvas* canvas, Color color);

  // Getters for various frame sizes.
  VideoFrame* natural_frame() { return natural_frame_.get(); }
  VideoFrame* larger_frame() { return larger_frame_.get(); }
  VideoFrame* smaller_frame() { return smaller_frame_.get(); }
  VideoFrame* cropped_frame() { return cropped_frame_.get(); }

  // Standard canvas.
  SkCanvas* target_canvas() { return &target_canvas_; }

 private:
  SkCanvasVideoRenderer renderer_;

  scoped_refptr<VideoFrame> natural_frame_;
  scoped_refptr<VideoFrame> larger_frame_;
  scoped_refptr<VideoFrame> smaller_frame_;
  scoped_refptr<VideoFrame> cropped_frame_;

  SkCanvas target_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SkCanvasVideoRendererTest);
};

static SkBitmap AllocBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType));
  bitmap.eraseColor(0);
  return bitmap;
}

SkCanvasVideoRendererTest::SkCanvasVideoRendererTest()
    : natural_frame_(VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight))),
      larger_frame_(VideoFrame::CreateBlackFrame(
          gfx::Size(kWidth * 2, kHeight * 2))),
      smaller_frame_(VideoFrame::CreateBlackFrame(
          gfx::Size(kWidth / 2, kHeight / 2))),
      cropped_frame_(VideoFrame::CreateFrame(
          VideoFrame::YV12,
          gfx::Size(16, 16),
          gfx::Rect(6, 6, 8, 6),
          gfx::Size(8, 6),
          base::TimeDelta::FromMilliseconds(4))),
      target_canvas_(AllocBitmap(kWidth, kHeight)) {
  // Give each frame a unique timestamp.
  natural_frame_->set_timestamp(base::TimeDelta::FromMilliseconds(1));
  larger_frame_->set_timestamp(base::TimeDelta::FromMilliseconds(2));
  smaller_frame_->set_timestamp(base::TimeDelta::FromMilliseconds(3));

  // Make sure the cropped video frame's aspect ratio matches the output device.
  // Update cropped_frame_'s crop dimensions if this is not the case.
  EXPECT_EQ(cropped_frame()->visible_rect().width() * kHeight,
            cropped_frame()->visible_rect().height() * kWidth);

  // Fill in the cropped frame's entire data with colors:
  //
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //
  // The visible crop of the frame (as set by its visible_rect_) has contents:
  //
  //   Bl Bl R  R  R  R  R  R
  //   Bl Bl R  R  R  R  R  R
  //   G  G  B  B  B  B  B  B
  //   G  G  B  B  B  B  B  B
  //   G  G  B  B  B  B  B  B
  //   G  G  B  B  B  B  B  B
  //
  // Each color region in the cropped frame is on a 2x2 block granularity, to
  // avoid sharing UV samples between regions.

  static const uint8 cropped_y_plane[] = {
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0, 76, 76, 76, 76, 76, 76, 76, 76,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
    149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
  };

  static const uint8 cropped_u_plane[] = {
    128, 128, 128, 128,  84,  84,  84,  84,
    128, 128, 128, 128,  84,  84,  84,  84,
    128, 128, 128, 128,  84,  84,  84,  84,
    128, 128, 128, 128,  84,  84,  84,  84,
     43,  43,  43,  43, 255, 255, 255, 255,
     43,  43,  43,  43, 255, 255, 255, 255,
     43,  43,  43,  43, 255, 255, 255, 255,
     43,  43,  43,  43, 255, 255, 255, 255,
  };
  static const uint8 cropped_v_plane[] = {
    128, 128, 128, 128, 255, 255, 255, 255,
    128, 128, 128, 128, 255, 255, 255, 255,
    128, 128, 128, 128, 255, 255, 255, 255,
    128, 128, 128, 128, 255, 255, 255, 255,
     21,  21,  21,  21, 107, 107, 107, 107,
     21,  21,  21,  21, 107, 107, 107, 107,
     21,  21,  21,  21, 107, 107, 107, 107,
     21,  21,  21,  21, 107, 107, 107, 107,
  };

  media::CopyYPlane(cropped_y_plane, 16, 16, cropped_frame());
  media::CopyUPlane(cropped_u_plane, 8, 8, cropped_frame());
  media::CopyVPlane(cropped_v_plane, 8, 8, cropped_frame());
}

SkCanvasVideoRendererTest::~SkCanvasVideoRendererTest() {}

void SkCanvasVideoRendererTest::PaintWithoutFrame(SkCanvas* canvas) {
  renderer_.Paint(NULL, canvas, kNaturalRect, 0xFF);
}

void SkCanvasVideoRendererTest::Paint(VideoFrame* video_frame,
                                      SkCanvas* canvas,
                                      Color color) {
  switch (color) {
    case kNone:
      break;
    case kRed:
      media::FillYUV(video_frame, 76, 84, 255);
      break;
    case kGreen:
      media::FillYUV(video_frame, 149, 43, 21);
      break;
    case kBlue:
      media::FillYUV(video_frame, 29, 255, 107);
      break;
  }
  renderer_.Paint(video_frame, canvas, kNaturalRect, 0xFF);
}

TEST_F(SkCanvasVideoRendererTest, NoFrame) {
  // Test that black gets painted over canvas.
  FillCanvas(target_canvas(), SK_ColorRED);
  PaintWithoutFrame(target_canvas());
  EXPECT_EQ(SK_ColorBLACK, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, Natural) {
  Paint(natural_frame(), target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, Larger) {
  Paint(natural_frame(), target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(target_canvas()));

  Paint(larger_frame(), target_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, Smaller) {
  Paint(natural_frame(), target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(target_canvas()));

  Paint(smaller_frame(), target_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, NoTimestamp) {
  VideoFrame* video_frame = natural_frame();
  video_frame->set_timestamp(media::kNoTimestamp());
  Paint(video_frame, target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, SameVideoFrame) {
  Paint(natural_frame(), target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(target_canvas()));

  // Slow paints can get cached, expect the old color value.
  Paint(natural_frame(), target_canvas(), kBlue);
  EXPECT_EQ(SK_ColorRED, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, CroppedFrame) {
  Paint(cropped_frame(), target_canvas(), kNone);
  // Check the corners.
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(target_canvas(), 0, 0));
  EXPECT_EQ(SK_ColorRED,   GetColorAt(target_canvas(), kWidth - 1, 0));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(target_canvas(), 0, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE,  GetColorAt(target_canvas(), kWidth - 1,
                                                       kHeight - 1));
  // Check the interior along the border between color regions.  Note that we're
  // bilinearly upscaling, so we'll need to take care to pick sample points that
  // are just outside the "zone of resampling".
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(target_canvas(), kWidth  * 1 / 8 - 1,
                                                       kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorRED,   GetColorAt(target_canvas(), kWidth  * 3 / 8,
                                                       kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(target_canvas(), kWidth  * 1 / 8 - 1,
                                                       kHeight * 3 / 6));
  EXPECT_EQ(SK_ColorBLUE,  GetColorAt(target_canvas(), kWidth  * 3 / 8,
                                                       kHeight * 3 / 6));
}

TEST_F(SkCanvasVideoRendererTest, CroppedFrame_NoScaling) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  const gfx::Rect crop_rect = cropped_frame()->visible_rect();

  // Force painting to a non-zero position on the destination bitmap, to check
  // if the coordinates are calculated properly.
  const int offset_x = 10;
  const int offset_y = 15;
  canvas.translate(offset_x, offset_y);

  // Create a destination canvas with dimensions and scale which would not
  // cause scaling.
  canvas.scale(static_cast<SkScalar>(crop_rect.width()) / kWidth,
               static_cast<SkScalar>(crop_rect.height()) / kHeight);

  Paint(cropped_frame(), &canvas, kNone);

  // Check the corners.
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, offset_x, offset_y));
  EXPECT_EQ(SK_ColorRED,
            GetColorAt(&canvas, offset_x + crop_rect.width() - 1, offset_y));
  EXPECT_EQ(SK_ColorGREEN,
            GetColorAt(&canvas, offset_x, offset_y + crop_rect.height() - 1));
  EXPECT_EQ(SK_ColorBLUE,
            GetColorAt(&canvas,
                       offset_x + crop_rect.width() - 1,
                       offset_y + crop_rect.height() - 1));
}

}  // namespace media
