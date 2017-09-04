// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "skia/ext/analysis_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"

namespace {

void SolidColorFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(255, 255, 255, 255));
}

void TransparentFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));
}

} // namespace
namespace skia {

TEST(AnalysisCanvasTest, EmptyCanvas) {
  skia::AnalysisCanvas canvas(255, 255);

  SkColor color;
  EXPECT_TRUE(canvas.GetColorIfSolid(&color));
  EXPECT_EQ(color, SkColorSetARGB(0, 0, 0, 0));
}

TEST(AnalysisCanvasTest, ClearCanvas) {
  skia::AnalysisCanvas canvas(255, 255);

  // Transparent color
  SkColor color = SkColorSetARGB(0, 12, 34, 56);
  canvas.clear(color);

  SkColor outputColor;
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  // Solid color
  color = SkColorSetARGB(255, 65, 43, 21);
  canvas.clear(color);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(outputColor, color);

  // Translucent color
  color = SkColorSetARGB(128, 11, 22, 33);
  canvas.clear(color);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Test helper methods
  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  TransparentFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
}

TEST(AnalysisCanvasTest, ComplexActions) {
  skia::AnalysisCanvas canvas(255, 255);

  // Draw paint test.
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  SkPaint paint;
  paint.setColor(color);

  canvas.drawPaint(paint);

  SkColor outputColor;
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));

  // Draw points test.
  SkPoint points[4] = {
    SkPoint::Make(0, 0),
    SkPoint::Make(255, 0),
    SkPoint::Make(255, 255),
    SkPoint::Make(0, 255)
  };

  SolidColorFill(canvas);
  canvas.drawPoints(SkCanvas::kLines_PointMode, 4, points, paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Draw oval test.
  SolidColorFill(canvas);
  canvas.drawOval(SkRect::MakeWH(255, 255), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Draw bitmap test.
  SolidColorFill(canvas);
  SkBitmap secondBitmap;
  secondBitmap.allocN32Pixels(255, 255);
  canvas.drawBitmap(secondBitmap, 0, 0);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, SimpleDrawRect) {
  skia::AnalysisCanvas canvas(255, 255);

  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  SkPaint paint;
  paint.setColor(color);
  canvas.clipRect(SkRect::MakeWH(255, 255));
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  SkColor outputColor;
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(color, outputColor);

  color = SkColorSetARGB(255, 22, 33, 44);
  paint.setColor(color);
  canvas.translate(-128, -128);
  canvas.drawRect(SkRect::MakeWH(382, 382), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  color = SkColorSetARGB(255, 33, 44, 55);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(color, outputColor);

  color = SkColorSetARGB(0, 0, 0, 0);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  // This test relies on canvas treating a paint with 0-color as a no-op
  // thus not changing its "is_solid" status.
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(outputColor, SkColorSetARGB(255, 33, 44, 55));

  color = SkColorSetARGB(128, 128, 128, 128);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  paint.setBlendMode(SkBlendMode::kClear);
  canvas.drawRect(SkRect::MakeWH(382, 382), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  canvas.translate(128, 128);
  color = SkColorSetARGB(255, 11, 22, 33);
  paint.setColor(color);
  paint.setBlendMode(SkBlendMode::kSrcOver);
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(color, outputColor);

  canvas.rotate(50);
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, FilterPaint) {
  skia::AnalysisCanvas canvas(255, 255);
  SkPaint paint;

  paint.setImageFilter(SkOffsetImageFilter::Make(10, 10, nullptr));
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  SkColor outputColor;
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, ClipPath) {
  skia::AnalysisCanvas canvas(255, 255);

  // Skia will look for paths that are actually rects and treat
  // them as such. We add a divot to the following path to prevent
  // this optimization and truly test clipPath's behavior.
  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(128, 50);
  path.lineTo(255, 0);
  path.lineTo(255, 255);
  path.lineTo(0, 255);

  SkColor outputColor;
  SolidColorFill(canvas);
  canvas.clipPath(path);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.save();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.clipPath(path);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.restore();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, SaveLayerWithXfermode) {
  skia::AnalysisCanvas canvas(255, 255);
  SkRect bounds = SkRect::MakeWH(255, 255);
  SkColor outputColor;

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  SkPaint paint;

  // Note: nothing is draw to the the save layer, but solid color
  // and transparency are handled conservatively in case the layer's
  // SkPaint draws something. For example, there could be an
  // SkPictureImageFilter. If someday analysis_canvas starts doing
  // a deeper analysis of the SkPaint, this test may need to be
  // redesigned.
  TransparentFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  paint.setBlendMode(SkBlendMode::kSrc);
  canvas.saveLayer(&bounds, &paint);
  canvas.restore();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  TransparentFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  paint.setBlendMode(SkBlendMode::kSrcOver);
  canvas.saveLayer(&bounds, &paint);
  canvas.restore();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Layer with dst xfermode is a no-op, so this is the only case
  // where solid color is unaffected by the layer.
  TransparentFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  paint.setBlendMode(SkBlendMode::kDst);
  canvas.saveLayer(&bounds, &paint);
  canvas.restore();
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
}

TEST(AnalysisCanvasTest, SaveLayerRestore) {
  skia::AnalysisCanvas canvas(255, 255);

  SkColor outputColor;
  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));

  SkRect bounds = SkRect::MakeWH(255, 255);
  SkPaint paint;
  paint.setColor(SkColorSetARGB(255, 255, 255, 255));
  paint.setBlendMode(SkBlendMode::kSrcOver);

  // This should force non-transparency
  canvas.saveLayer(&bounds, &paint);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  TransparentFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  paint.setBlendMode(SkBlendMode::kDst);

  // This should force non-solid color
  canvas.saveLayer(&bounds, &paint);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  TransparentFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.restore();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  TransparentFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  canvas.restore();
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  TransparentFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
}

TEST(AnalysisCanvasTest, EarlyOutNotSolid) {
  SkRTreeFactory factory;
  SkPictureRecorder recorder;

  // Create a picture with 3 commands, last of which is non-solid.
  sk_sp<SkCanvas> record_canvas = sk_ref_sp(recorder.beginRecording(256, 256, &factory));

  std::string text = "text";
  SkPoint point = SkPoint::Make(SkIntToScalar(25), SkIntToScalar(25));

  SkPaint paint;
  paint.setColor(SkColorSetARGB(255, 255, 255, 255));
  paint.setBlendMode(SkBlendMode::kSrcOver);

  record_canvas->drawRect(SkRect::MakeWH(256, 256), paint);
  record_canvas->drawRect(SkRect::MakeWH(256, 256), paint);
  record_canvas->drawText(
      text.c_str(), text.length(), point.fX, point.fY, paint);

  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

  // Draw the picture into the analysis canvas, using the canvas as a callback
  // as well.
  skia::AnalysisCanvas canvas(256, 256);
  picture->playback(&canvas, &canvas);

  // Ensure that canvas is not solid.
  SkColor output_color;
  EXPECT_FALSE(canvas.GetColorIfSolid(&output_color));

  // Verify that we aborted drawing.
  EXPECT_TRUE(canvas.abort());
}

TEST(AnalysisCanvasTest, ClipComplexRegion) {
  skia::AnalysisCanvas canvas(255, 255);

  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(128, 50);
  path.lineTo(255, 0);
  path.lineTo(255, 255);
  path.lineTo(0, 255);
  SkIRect pathBounds = path.getBounds().round();
  SkRegion region;
  region.setPath(path, SkRegion(pathBounds));

  SkColor outputColor;
  SolidColorFill(canvas);
  canvas.clipRegion(region);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.save();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.clipRegion(region);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.restore();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, ClipRRectCoversCanvas) {

  SkVector radii[4] = {
    SkVector::Make(10.0, 15.0),
    SkVector::Make(20.0, 25.0),
    SkVector::Make(30.0, 35.0),
    SkVector::Make(40.0, 45.0),
  };

  int rr_size = 600;
  int canvas_size = 255;

  struct {
    SkVector offset;
    bool expected;
  } cases [] = {
    // Not within bounding box of |rr|.
    { SkVector::Make(100.0, 100.0), false },

    // Intersects UL corner.
    { SkVector::Make(0.0, 0.0), false },

    // Between UL and UR.
    { SkVector::Make(-50.0, 0), true },

    // Intersects UR corner.
    { SkVector::Make(canvas_size - rr_size, 0), false },

    // Between UR and LR.
    { SkVector::Make(canvas_size - rr_size, -50.0), true },

    // Intersects LR corner.
    { SkVector::Make(canvas_size - rr_size, canvas_size - rr_size), false },

    // Between LL and LR
    { SkVector::Make(-50, canvas_size - rr_size), true },

    // Intersects LL corner
    { SkVector::Make(0, canvas_size - rr_size), false },

    // Between UL and LL
    { SkVector::Make(0, -50), true },

    // In center
    { SkVector::Make(-100, -100), true},
  };

  SkColor outputColor;

  for (size_t i = 0; i < arraysize(cases); ++i) {
    skia::AnalysisCanvas canvas(canvas_size, canvas_size);

    SkRect bounding_rect = SkRect::MakeXYWH(
        cases[i].offset.x(), cases[i].offset.y(), rr_size, rr_size);

    SkRRect rr;
    rr.setRectRadii(bounding_rect, radii);

    canvas.clipRRect(rr);
    EXPECT_EQ(cases[i].expected, canvas.GetColorIfSolid(&outputColor)) << i;
  }
}

}  // namespace skia
