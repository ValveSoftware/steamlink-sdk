// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/pixel_ref_utils.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/src/core/SkOrderedReadBuffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace skia {

namespace {

class TestImageGenerator : public SkImageGenerator {
  public:
   TestImageGenerator(const SkImageInfo& info)
     : SkImageGenerator(info) { }
};

sk_sp<SkImage> MakeDiscardableImage(const gfx::Size& size) {
  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(size.width(), size.height());
  return SkImage::MakeFromGenerator(new TestImageGenerator(info));
}

void SetDiscardableShader(SkPaint* paint) {
  sk_sp<SkImage> image = MakeDiscardableImage(gfx::Size(50, 50));
  paint->setShader(
      image->makeShader(SkShader::kClamp_TileMode, SkShader::kClamp_TileMode));
}

SkCanvas* StartRecording(SkPictureRecorder* recorder, gfx::Rect layer_rect) {
  SkCanvas* canvas =
      recorder->beginRecording(layer_rect.width(), layer_rect.height());

  canvas->save();
  canvas->translate(-layer_rect.x(), -layer_rect.y());
  canvas->clipRect(SkRect::MakeXYWH(
      layer_rect.x(), layer_rect.y(), layer_rect.width(), layer_rect.height()));

  return canvas;
}

sk_sp<SkPicture> StopRecording(SkPictureRecorder* recorder, SkCanvas* canvas) {
  canvas->restore();
  return recorder->finishRecordingAsPicture();
}

}  // namespace

void VerifyScales(SkScalar x_scale,
                  SkScalar y_scale,
                  const SkMatrix& matrix,
                  int source_line) {
  SkSize scales;
  bool success = matrix.decomposeScale(&scales);
  EXPECT_TRUE(success) << "line: " << source_line;
  EXPECT_FLOAT_EQ(x_scale, scales.width()) << "line: " << source_line;
  EXPECT_FLOAT_EQ(y_scale, scales.height()) << "line: " << source_line;
}

TEST(PixelRefUtilsTest, DrawPaint) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint second_paint;
  SetDiscardableShader(&second_paint);

  SkPaint third_paint;
  SetDiscardableShader(&third_paint);

  canvas->drawPaint(first_paint);
  canvas->clipRect(SkRect::MakeXYWH(34, 45, 56, 67));
  canvas->drawPaint(second_paint);

  canvas->save();
  canvas->scale(2.f, 3.f);
  canvas->drawPaint(second_paint);
  canvas->restore();

  // Total clip is now (34, 45, 56, 55)
  canvas->clipRect(SkRect::MakeWH(100, 100));
  canvas->drawPaint(third_paint);

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(4u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 256, 256),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(34, 45, 56, 67),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(34, 45, 56, 67),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(2.f, 3.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(34, 45, 56, 55),
                       gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[3].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[3].filter_quality);
}

TEST(PixelRefUtilsTest, DrawPoints) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint second_paint;
  SetDiscardableShader(&second_paint);

  SkPaint third_paint;
  SetDiscardableShader(&third_paint);

  SkPoint points[3];
  points[0].set(10, 10);
  points[1].set(100, 20);
  points[2].set(50, 100);
  // (10, 10, 90, 90).
  canvas->drawPoints(SkCanvas::kPolygon_PointMode, 3, points, first_paint);

  canvas->save();

  canvas->clipRect(SkRect::MakeWH(50, 50));
  // (10, 10, 90, 90).
  canvas->drawPoints(SkCanvas::kPolygon_PointMode, 3, points, second_paint);

  canvas->restore();

  points[0].set(50, 55);
  points[1].set(50, 55);
  points[2].set(200, 200);
  // (50, 55, 150, 145).
  canvas->drawPoints(SkCanvas::kPolygon_PointMode, 3, points, third_paint);

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 90, 90),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 90, 90),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 55, 150, 145),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
}

TEST(PixelRefUtilsTest, DrawRect) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint second_paint;
  SetDiscardableShader(&second_paint);

  SkPaint third_paint;
  SetDiscardableShader(&third_paint);

  // (10, 20, 30, 40).
  canvas->drawRect(SkRect::MakeXYWH(10, 20, 30, 40), first_paint);

  canvas->save();

  canvas->translate(5, 17);
  // (5, 50, 25, 35)
  canvas->drawRect(SkRect::MakeXYWH(0, 33, 25, 35), second_paint);

  canvas->restore();

  canvas->clipRect(SkRect::MakeXYWH(50, 50, 50, 50));
  canvas->translate(20, 20);
  // (20, 20, 100, 100)
  canvas->drawRect(SkRect::MakeXYWH(0, 0, 100, 100), third_paint);

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 20, 30, 40),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(5, 50, 25, 35),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(20, 20, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
}

TEST(PixelRefUtilsTest, DrawRRect) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint second_paint;
  SetDiscardableShader(&second_paint);

  SkPaint third_paint;
  SetDiscardableShader(&third_paint);

  SkRRect rrect;
  rrect.setRect(SkRect::MakeXYWH(10, 20, 30, 40));

  // (10, 20, 30, 40).
  canvas->drawRRect(rrect, first_paint);

  canvas->save();

  canvas->translate(5, 17);
  rrect.setRect(SkRect::MakeXYWH(0, 33, 25, 35));
  // (5, 50, 25, 35)
  canvas->drawRRect(rrect, second_paint);

  canvas->restore();

  canvas->clipRect(SkRect::MakeXYWH(50, 50, 50, 50));
  canvas->translate(20, 20);
  rrect.setRect(SkRect::MakeXYWH(0, 0, 100, 100));
  // (20, 20, 100, 100)
  canvas->drawRRect(rrect, third_paint);

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 20, 30, 40),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(5, 50, 25, 35),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(20, 20, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
}

TEST(PixelRefUtilsTest, DrawOval) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint second_paint;
  SetDiscardableShader(&second_paint);

  SkPaint third_paint;
  SetDiscardableShader(&third_paint);

  canvas->save();

  canvas->scale(2.f, 0.5f);
  // (20, 10, 60, 20).
  canvas->drawOval(SkRect::MakeXYWH(10, 20, 30, 40), first_paint);

  canvas->restore();
  canvas->save();

  canvas->translate(1, 2);
  // (1, 35, 25, 35)
  canvas->drawRect(SkRect::MakeXYWH(0, 33, 25, 35), second_paint);

  canvas->restore();

  canvas->clipRect(SkRect::MakeXYWH(50, 50, 50, 50));
  canvas->translate(20, 20);
  // (20, 20, 100, 100).
  canvas->drawRect(SkRect::MakeXYWH(0, 0, 100, 100), third_paint);

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(20, 10, 60, 20),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(2.f, 0.5f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(1, 35, 25, 35),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(20, 20, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
}

TEST(PixelRefUtilsTest, DrawPath) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint second_paint;
  SetDiscardableShader(&second_paint);

  SkPath path;
  path.moveTo(12, 13);
  path.lineTo(50, 50);
  path.lineTo(22, 101);

  // (12, 13, 38, 88).
  canvas->drawPath(path, first_paint);

  canvas->save();
  canvas->clipRect(SkRect::MakeWH(50, 50));

  // (12, 13, 38, 88), since clips are ignored as long as the shape is in the
  // clip.
  canvas->drawPath(path, second_paint);

  canvas->restore();

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(2u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(12, 13, 38, 88),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(12, 13, 38, 88),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
}

TEST(PixelRefUtilsTest, DrawText) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPoint points[4];
  points[0].set(10, 50);
  points[1].set(20, 50);
  points[2].set(30, 50);
  points[3].set(40, 50);

  SkPath path;
  path.moveTo(10, 50);
  path.lineTo(20, 50);
  path.lineTo(30, 50);
  path.lineTo(40, 50);
  path.lineTo(50, 50);

  canvas->drawText("text", 4, 50, 50, first_paint);
  canvas->drawPosText("text", 4, points, first_paint);
  canvas->drawTextOnPath("text", 4, path, NULL, first_paint);

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
}

TEST(PixelRefUtilsTest, DrawVertices) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint second_paint;
  SetDiscardableShader(&second_paint);

  SkPaint third_paint;
  SetDiscardableShader(&third_paint);

  SkPoint points[3];
  SkColor colors[3];
  uint16_t indecies[3] = {0, 1, 2};
  points[0].set(10, 10);
  points[1].set(100, 20);
  points[2].set(50, 100);
  // (10, 10, 90, 90).
  canvas->drawVertices(SkCanvas::kTriangles_VertexMode,
                       3,
                       points,
                       points,
                       colors,
                       NULL,
                       indecies,
                       3,
                       first_paint);

  canvas->save();

  canvas->clipRect(SkRect::MakeWH(50, 50));
  // (10, 10, 90, 90), since clips are ignored as long as the draw object is
  // within clip.
  canvas->drawVertices(SkCanvas::kTriangles_VertexMode,
                       3,
                       points,
                       points,
                       colors,
                       NULL,
                       indecies,
                       3,
                       second_paint);

  canvas->restore();

  points[0].set(50, 55);
  points[1].set(50, 55);
  points[2].set(200, 200);
  // (50, 55, 150, 145).
  canvas->drawVertices(SkCanvas::kTriangles_VertexMode,
                       3,
                       points,
                       points,
                       colors,
                       NULL,
                       indecies,
                       3,
                       third_paint);

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 90, 90),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 90, 90),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 55, 150, 145),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
}

TEST(PixelRefUtilsTest, DrawImage) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  sk_sp<SkImage> first = MakeDiscardableImage(gfx::Size(50, 50));
  sk_sp<SkImage> second = MakeDiscardableImage(gfx::Size(50, 50));
  sk_sp<SkImage> third = MakeDiscardableImage(gfx::Size(50, 50));
  sk_sp<SkImage> fourth = MakeDiscardableImage(gfx::Size(50, 1));
  sk_sp<SkImage> fifth = MakeDiscardableImage(gfx::Size(10, 10));
  sk_sp<SkImage> sixth = MakeDiscardableImage(gfx::Size(10, 10));

  canvas->save();

  // At (0, 0).
  canvas->drawImage(first.get(), 0, 0);
  canvas->translate(25, 0);
  // At (25, 0).
  canvas->drawImage(second.get(), 0, 0);
  canvas->translate(0, 50);
  // At (50, 50).
  canvas->drawImage(third.get(), 25, 0);

  canvas->restore();
  canvas->save();

  canvas->translate(1, 0);
  canvas->rotate(90);
  // At (1, 0), rotated 90 degrees
  canvas->drawImage(fourth.get(), 0, 0);

  canvas->restore();
  canvas->save();

  canvas->scale(5.f, 6.f);
  // At (0, 0), scaled by 5 and 6
  canvas->drawImage(fifth.get(), 0, 0);

  canvas->restore();

  canvas->rotate(27);
  canvas->scale(3.3f, 0.4f);

  canvas->drawImage(sixth.get(), 0, 0);

  canvas->restore();

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(6u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(25, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 50, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 1, 50),
                       gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect));
  VerifyScales(1.f, 1.f, pixel_refs[3].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[3].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 60),
                       gfx::SkRectToRectF(pixel_refs[4].pixel_ref_rect));
  VerifyScales(5.f, 6.f, pixel_refs[4].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[4].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(-1.8159621f, 0, 31.219175f, 18.545712f),
                       gfx::SkRectToRectF(pixel_refs[5].pixel_ref_rect));
  VerifyScales(3.3f, 0.4f, pixel_refs[5].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[5].filter_quality);
}

TEST(PixelRefUtilsTest, DrawImageRect) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  sk_sp<SkImage> first = MakeDiscardableImage(gfx::Size(50, 50));
  sk_sp<SkImage> second = MakeDiscardableImage(gfx::Size(50, 50));
  sk_sp<SkImage> third = MakeDiscardableImage(gfx::Size(50, 50));

  SkPaint first_paint;
  SetDiscardableShader(&first_paint);

  SkPaint non_discardable_paint;

  canvas->save();

  // (0, 0, 100, 100).
  canvas->drawImageRect(
      first.get(), SkRect::MakeWH(100, 100), &non_discardable_paint);
  canvas->translate(25, 0);
  // (75, 50, 10, 10).
  canvas->drawImageRect(
      second.get(), SkRect::MakeXYWH(50, 50, 10, 10), &non_discardable_paint);
  canvas->translate(5, 50);
  // (0, 30, 100, 100). One from bitmap, one from paint.
  canvas->drawImageRect(
      third.get(), SkRect::MakeXYWH(-30, -20, 100, 100), &first_paint);

  canvas->restore();

  sk_sp<SkPicture> picture = StopRecording(&recorder, canvas);

  std::vector<skia::PixelRefUtils::PositionPixelRef> pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(4u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  VerifyScales(2.f, 2.f, pixel_refs[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(75, 50, 10, 10),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  VerifyScales(0.2f, 0.2f, pixel_refs[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 30, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  VerifyScales(2.f, 2.f, pixel_refs[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[2].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 30, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect));
  VerifyScales(2.f, 2.f, pixel_refs[3].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, pixel_refs[3].filter_quality);
}

}  // namespace skia
