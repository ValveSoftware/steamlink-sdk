// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/message_loop/message_loop.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/geometry/rect_f.h"

using media::VideoFrame;

namespace media {

static const int kWidth = 320;
static const int kHeight = 240;
static const gfx::RectF kNaturalRect(kWidth, kHeight);

// Helper for filling a |canvas| with a solid |color|.
void FillCanvas(SkCanvas* canvas, SkColor color) {
  canvas->clear(color);
}

// Helper for returning the color of a solid |canvas|.
SkColor GetColorAt(SkCanvas* canvas, int x, int y) {
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(1, 1))
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
  ~SkCanvasVideoRendererTest() override;

  // Paints to |canvas| using |renderer_| without any frame data.
  void PaintWithoutFrame(SkCanvas* canvas);

  // Paints the |video_frame| to the |canvas| using |renderer_|, setting the
  // color of |video_frame| to |color| first.
  void Paint(const scoped_refptr<VideoFrame>& video_frame,
             SkCanvas* canvas,
             Color color);
  void PaintRotated(const scoped_refptr<VideoFrame>& video_frame,
                    SkCanvas* canvas,
                    const gfx::RectF& dest_rect,
                    Color color,
                    SkBlendMode mode,
                    VideoRotation video_rotation);

  void Copy(const scoped_refptr<VideoFrame>& video_frame, SkCanvas* canvas);

  // Getters for various frame sizes.
  scoped_refptr<VideoFrame> natural_frame() { return natural_frame_; }
  scoped_refptr<VideoFrame> larger_frame() { return larger_frame_; }
  scoped_refptr<VideoFrame> smaller_frame() { return smaller_frame_; }
  scoped_refptr<VideoFrame> cropped_frame() { return cropped_frame_; }

  // Standard canvas.
  SkCanvas* target_canvas() { return &target_canvas_; }

 protected:
  SkCanvasVideoRenderer renderer_;

  scoped_refptr<VideoFrame> natural_frame_;
  scoped_refptr<VideoFrame> larger_frame_;
  scoped_refptr<VideoFrame> smaller_frame_;
  scoped_refptr<VideoFrame> cropped_frame_;

  SkCanvas target_canvas_;
  base::MessageLoop message_loop_;

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
      larger_frame_(
          VideoFrame::CreateBlackFrame(gfx::Size(kWidth * 2, kHeight * 2))),
      smaller_frame_(
          VideoFrame::CreateBlackFrame(gfx::Size(kWidth / 2, kHeight / 2))),
      cropped_frame_(
          VideoFrame::CreateFrame(PIXEL_FORMAT_YV12,
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

  static const uint8_t cropped_y_plane[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
  };

  static const uint8_t cropped_u_plane[] = {
      128, 128, 128, 128, 84,  84,  84,  84,  128, 128, 128, 128, 84,
      84,  84,  84,  128, 128, 128, 128, 84,  84,  84,  84,  128, 128,
      128, 128, 84,  84,  84,  84,  43,  43,  43,  43,  255, 255, 255,
      255, 43,  43,  43,  43,  255, 255, 255, 255, 43,  43,  43,  43,
      255, 255, 255, 255, 43,  43,  43,  43,  255, 255, 255, 255,
  };
  static const uint8_t cropped_v_plane[] = {
      128, 128, 128, 128, 255, 255, 255, 255, 128, 128, 128, 128, 255,
      255, 255, 255, 128, 128, 128, 128, 255, 255, 255, 255, 128, 128,
      128, 128, 255, 255, 255, 255, 21,  21,  21,  21,  107, 107, 107,
      107, 21,  21,  21,  21,  107, 107, 107, 107, 21,  21,  21,  21,
      107, 107, 107, 107, 21,  21,  21,  21,  107, 107, 107, 107,
  };

  libyuv::I420Copy(cropped_y_plane, 16, cropped_u_plane, 8, cropped_v_plane, 8,
                   cropped_frame()->data(VideoFrame::kYPlane),
                   cropped_frame()->stride(VideoFrame::kYPlane),
                   cropped_frame()->data(VideoFrame::kUPlane),
                   cropped_frame()->stride(VideoFrame::kUPlane),
                   cropped_frame()->data(VideoFrame::kVPlane),
                   cropped_frame()->stride(VideoFrame::kVPlane), 16, 16);
}

SkCanvasVideoRendererTest::~SkCanvasVideoRendererTest() {}

void SkCanvasVideoRendererTest::PaintWithoutFrame(SkCanvas* canvas) {
  SkPaint paint;
  paint.setFilterQuality(kLow_SkFilterQuality);
  renderer_.Paint(nullptr, canvas, kNaturalRect, paint, VIDEO_ROTATION_0,
                  Context3D());
}

void SkCanvasVideoRendererTest::Paint(
    const scoped_refptr<VideoFrame>& video_frame,
    SkCanvas* canvas,
    Color color) {
  PaintRotated(video_frame, canvas, kNaturalRect, color, SkBlendMode::kSrcOver,
               VIDEO_ROTATION_0);
}

void SkCanvasVideoRendererTest::PaintRotated(
    const scoped_refptr<VideoFrame>& video_frame,
    SkCanvas* canvas,
    const gfx::RectF& dest_rect,
    Color color,
    SkBlendMode mode,
    VideoRotation video_rotation) {
  switch (color) {
    case kNone:
      break;
    case kRed:
      media::FillYUV(video_frame.get(), 76, 84, 255);
      break;
    case kGreen:
      media::FillYUV(video_frame.get(), 149, 43, 21);
      break;
    case kBlue:
      media::FillYUV(video_frame.get(), 29, 255, 107);
      break;
  }
  SkPaint paint;
  paint.setBlendMode(mode);
  paint.setFilterQuality(kLow_SkFilterQuality);
  renderer_.Paint(video_frame, canvas, dest_rect, paint, video_rotation,
                  Context3D());
}

void SkCanvasVideoRendererTest::Copy(
    const scoped_refptr<VideoFrame>& video_frame,
    SkCanvas* canvas) {
  renderer_.Copy(video_frame, canvas, Context3D());
}

TEST_F(SkCanvasVideoRendererTest, NoFrame) {
  // Test that black gets painted over canvas.
  FillCanvas(target_canvas(), SK_ColorRED);
  PaintWithoutFrame(target_canvas());
  EXPECT_EQ(SK_ColorBLACK, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, TransparentFrame) {
  FillCanvas(target_canvas(), SK_ColorRED);
  PaintRotated(
      VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
      target_canvas(), kNaturalRect, kNone, SkBlendMode::kSrcOver,
      VIDEO_ROTATION_0);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorRED), GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, TransparentFrameSrcMode) {
  FillCanvas(target_canvas(), SK_ColorRED);
  // SRC mode completely overwrites the buffer.
  PaintRotated(
      VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
      target_canvas(), kNaturalRect, kNone, SkBlendMode::kSrc,
      VIDEO_ROTATION_0);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, CopyTransparentFrame) {
  FillCanvas(target_canvas(), SK_ColorRED);
  Copy(VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
       target_canvas());
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            GetColor(target_canvas()));
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
  VideoFrame* video_frame = natural_frame().get();
  video_frame->set_timestamp(media::kNoTimestamp);
  Paint(video_frame, target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(target_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, CroppedFrame) {
  Paint(cropped_frame(), target_canvas(), kNone);
  // Check the corners.
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(target_canvas(), 0, 0));
  EXPECT_EQ(SK_ColorRED, GetColorAt(target_canvas(), kWidth - 1, 0));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(target_canvas(), 0, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(target_canvas(), kWidth - 1, kHeight - 1));
  // Check the interior along the border between color regions.  Note that we're
  // bilinearly upscaling, so we'll need to take care to pick sample points that
  // are just outside the "zone of resampling".
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(target_canvas(), kWidth * 1 / 8 - 1,
                                      kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorRED,
            GetColorAt(target_canvas(), kWidth * 3 / 8, kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorGREEN,
            GetColorAt(target_canvas(), kWidth * 1 / 8 - 1, kHeight * 3 / 6));
  EXPECT_EQ(SK_ColorBLUE,
            GetColorAt(target_canvas(), kWidth * 3 / 8, kHeight * 3 / 6));
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
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, offset_x + crop_rect.width() - 1,
                                     offset_y + crop_rect.height() - 1));
}

TEST_F(SkCanvasVideoRendererTest, Video_Rotation_90) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  PaintRotated(cropped_frame(), &canvas, kNaturalRect, kNone,
               SkBlendMode::kSrcOver, VIDEO_ROTATION_90);
  // Check the corners.
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(&canvas, 0, 0));
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, kWidth - 1, 0));
  EXPECT_EQ(SK_ColorRED, GetColorAt(&canvas, kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, 0, kHeight - 1));
}

TEST_F(SkCanvasVideoRendererTest, Video_Rotation_180) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  PaintRotated(cropped_frame(), &canvas, kNaturalRect, kNone,
               SkBlendMode::kSrcOver, VIDEO_ROTATION_180);
  // Check the corners.
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, 0, 0));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(&canvas, kWidth - 1, 0));
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorRED, GetColorAt(&canvas, 0, kHeight - 1));
}

TEST_F(SkCanvasVideoRendererTest, Video_Rotation_270) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  PaintRotated(cropped_frame(), &canvas, kNaturalRect, kNone,
               SkBlendMode::kSrcOver, VIDEO_ROTATION_270);
  // Check the corners.
  EXPECT_EQ(SK_ColorRED, GetColorAt(&canvas, 0, 0));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, kWidth - 1, 0));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(&canvas, kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, 0, kHeight - 1));
}

TEST_F(SkCanvasVideoRendererTest, Video_Translate) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  FillCanvas(&canvas, SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_0);
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, (kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            GetColorAt(&canvas, (kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorRED, GetColorAt(&canvas, kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(&canvas, kWidth / 2, kHeight - 1));
}

TEST_F(SkCanvasVideoRendererTest, Video_Translate_Rotation_90) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  FillCanvas(&canvas, SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_90);
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, (kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            GetColorAt(&canvas, (kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(&canvas, kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorRED, GetColorAt(&canvas, kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, kWidth / 2, kHeight - 1));
}

TEST_F(SkCanvasVideoRendererTest, Video_Translate_Rotation_180) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  FillCanvas(&canvas, SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_180);
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, (kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            GetColorAt(&canvas, (kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(&canvas, kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorRED, GetColorAt(&canvas, kWidth / 2, kHeight - 1));
}

TEST_F(SkCanvasVideoRendererTest, Video_Translate_Rotation_270) {
  SkCanvas canvas(AllocBitmap(kWidth, kHeight));
  FillCanvas(&canvas, SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_270);
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, (kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            GetColorAt(&canvas, (kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, GetColorAt(&canvas, 0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorRED, GetColorAt(&canvas, kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(&canvas, kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(&canvas, kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(&canvas, kWidth / 2, kHeight - 1));
}

TEST_F(SkCanvasVideoRendererTest, HighBits) {
  // Copy cropped_frame into a highbit frame.
  scoped_refptr<VideoFrame> frame(VideoFrame::CreateFrame(
      PIXEL_FORMAT_YUV420P10, cropped_frame()->coded_size(),
      cropped_frame()->visible_rect(), cropped_frame()->natural_size(),
      cropped_frame()->timestamp()));
  for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane; ++plane) {
    int width = cropped_frame()->row_bytes(plane);
    uint16_t* dst = reinterpret_cast<uint16_t*>(frame->data(plane));
    uint8_t* src = cropped_frame()->data(plane);
    for (int row = 0; row < cropped_frame()->rows(plane); row++) {
      for (int col = 0; col < width; col++) {
        dst[col] = src[col] << 2;
      }
      src += cropped_frame()->stride(plane);
      dst += frame->stride(plane) / 2;
    }
  }

  Paint(frame, target_canvas(), kNone);
  // Check the corners.
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(target_canvas(), 0, 0));
  EXPECT_EQ(SK_ColorRED, GetColorAt(target_canvas(), kWidth - 1, 0));
  EXPECT_EQ(SK_ColorGREEN, GetColorAt(target_canvas(), 0, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE, GetColorAt(target_canvas(), kWidth - 1, kHeight - 1));
  // Check the interior along the border between color regions.  Note that we're
  // bilinearly upscaling, so we'll need to take care to pick sample points that
  // are just outside the "zone of resampling".
  EXPECT_EQ(SK_ColorBLACK, GetColorAt(target_canvas(), kWidth * 1 / 8 - 1,
                                      kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorRED,
            GetColorAt(target_canvas(), kWidth * 3 / 8, kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorGREEN,
            GetColorAt(target_canvas(), kWidth * 1 / 8 - 1, kHeight * 3 / 6));
  EXPECT_EQ(SK_ColorBLUE,
            GetColorAt(target_canvas(), kWidth * 3 / 8, kHeight * 3 / 6));
}

TEST_F(SkCanvasVideoRendererTest, Y16) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(16, 16, kPremul_SkAlphaType));

  // |offset_x| and |offset_y| define visible rect's offset to coded rect.
  const int offset_x = 3;
  const int offset_y = 5;
  const int stride = bitmap.width() + offset_x;
  const size_t byte_size = stride * (bitmap.height() + offset_y) * 2;
  std::unique_ptr<unsigned char, base::AlignedFreeDeleter> memory(
      static_cast<unsigned char*>(base::AlignedAlloc(
          byte_size, media::VideoFrame::kFrameAddressAlignment)));

  // In the visible rect, fill upper byte with [0-255] and lower with [255-0].
  uint16_t* data = reinterpret_cast<uint16_t*>(memory.get());
  for (int j = 0; j < bitmap.height(); j++) {
    for (int i = 0; i < bitmap.width(); i++) {
      const int value = i + j * bitmap.width();
      data[(stride * (j + offset_y)) + i + offset_x] =
          ((value & 0xFF) << 8) | (~value & 0xFF);
    }
  }
  const gfx::Rect rect(offset_x, offset_y, bitmap.width(), bitmap.height());
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapExternalData(
          media::PIXEL_FORMAT_Y16,
          gfx::Size(stride, offset_y + bitmap.height()), rect, rect.size(),
          memory.get(), byte_size, cropped_frame()->timestamp());

  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setFilterQuality(kNone_SkFilterQuality);
  renderer_.Paint(video_frame, &canvas,
                  gfx::RectF(bitmap.width(), bitmap.height()), paint,
                  VIDEO_ROTATION_0, Context3D());
  SkAutoLockPixels lock(bitmap);
  for (int j = 0; j < bitmap.height(); j++) {
    for (int i = 0; i < bitmap.width(); i++) {
      const int value = i + j * bitmap.width();
      EXPECT_EQ(SkColorSetRGB(value, value, value), bitmap.getColor(i, j));
    }
  }
}

namespace {
class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  void GenTextures(GLsizei n, GLuint* textures) override {
    DCHECK_EQ(1, n);
    *textures = 1;
  }
};
void MailboxHoldersReleased(const gpu::SyncToken& sync_token) {}
}  // namespace

// Test that SkCanvasVideoRendererTest::Paint doesn't crash when GrContext is
// abandoned.
TEST_F(SkCanvasVideoRendererTest, ContextLost) {
  sk_sp<const GrGLInterface> null_interface(GrGLCreateNullInterface());
  sk_sp<GrContext> gr_context(GrContext::Create(
      kOpenGL_GrBackend,
      reinterpret_cast<GrBackendContext>(null_interface.get())));
  gr_context->abandonContext();

  SkCanvas canvas(AllocBitmap(kWidth, kHeight));

  TestGLES2Interface gles2;
  Context3D context_3d(&gles2, gr_context.get());
  gfx::Size size(kWidth, kHeight);
  gpu::MailboxHolder holders[VideoFrame::kMaxPlanes] = {gpu::MailboxHolder(
      gpu::Mailbox::Generate(), gpu::SyncToken(), GL_TEXTURE_RECTANGLE_ARB)};
  auto video_frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_UYVY, holders, base::Bind(MailboxHoldersReleased), size,
      gfx::Rect(size), size, kNoTimestamp);

  SkPaint paint;
  paint.setFilterQuality(kLow_SkFilterQuality);
  renderer_.Paint(video_frame, &canvas, kNaturalRect, paint, VIDEO_ROTATION_90,
                  context_3d);
}

void EmptyCallback(const gpu::SyncToken& sync_token) {}

TEST_F(SkCanvasVideoRendererTest, CorrectFrameSizeToVisibleRect) {
  int fWidth{16}, fHeight{16};
  SkImageInfo imInfo =
      SkImageInfo::MakeN32(fWidth, fHeight, kOpaque_SkAlphaType);

  sk_sp<const GrGLInterface> glInterface(GrGLCreateNullInterface());
  sk_sp<GrContext> grContext(
      GrContext::Create(kOpenGL_GrBackend,
                        reinterpret_cast<GrBackendContext>(glInterface.get())));

  sk_sp<SkSurface> skSurface =
      SkSurface::MakeRenderTarget(grContext.get(), SkBudgeted::kYes, imInfo);
  SkCanvas* canvas = skSurface->getCanvas();

  TestGLES2Interface gles2;
  Context3D context_3d(&gles2, grContext.get());
  gfx::Size coded_size(fWidth, fHeight);
  gfx::Size visible_size(fWidth / 2, fHeight / 2);

  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  for (size_t i = 0; i < VideoFrame::kMaxPlanes; i++) {
    mailbox_holders[i] = gpu::MailboxHolder(
        gpu::Mailbox::Generate(), gpu::SyncToken(), GL_TEXTURE_RECTANGLE_ARB);
  }

  auto video_frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_I420, mailbox_holders, base::Bind(EmptyCallback), coded_size,
      gfx::Rect(visible_size), visible_size,
      base::TimeDelta::FromMilliseconds(4));

  gfx::RectF visible_rect(visible_size.width(), visible_size.height());
  SkPaint paint;
  renderer_.Paint(video_frame, canvas, visible_rect, paint, VIDEO_ROTATION_0,
                  context_3d);

  EXPECT_EQ(fWidth / 2, renderer_.LastImageDimensionsForTesting().width());
  EXPECT_EQ(fWidth / 2, renderer_.LastImageDimensionsForTesting().height());
}

}  // namespace media
