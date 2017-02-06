// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include <cmath>
#include <limits>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace gfx {

Canvas::Canvas(const Size& size, float image_scale, bool is_opaque)
    : image_scale_(image_scale) {
  Size pixel_size = ScaleToCeiledSize(size, image_scale);
  canvas_ = sk_sp<SkCanvas>(skia::CreatePlatformCanvas(pixel_size.width(),
                                                       pixel_size.height(),
                                                       is_opaque));
#if !defined(USE_CAIRO)
  // skia::PlatformCanvas instances are initialized to 0 by Cairo, but
  // uninitialized on other platforms.
  if (!is_opaque)
    canvas_->clear(SkColorSetARGB(0, 0, 0, 0));
#endif

  SkScalar scale_scalar = SkFloatToScalar(image_scale);
  canvas_->scale(scale_scalar, scale_scalar);
}

Canvas::Canvas()
    : image_scale_(1.f),
      canvas_(sk_sp<SkCanvas>(skia::CreatePlatformCanvas(0, 0, false))) {}

Canvas::Canvas(sk_sp<SkCanvas> canvas, float image_scale)
    : image_scale_(image_scale), canvas_(std::move(canvas)) {
  DCHECK(canvas_);
}

Canvas::~Canvas() {
}

void Canvas::RecreateBackingCanvas(const Size& size,
                                   float image_scale,
                                   bool is_opaque) {
  image_scale_ = image_scale;
  Size pixel_size = ScaleToFlooredSize(size, image_scale);
  canvas_ = sk_sp<SkCanvas>(skia::CreatePlatformCanvas(pixel_size.width(),
                                                       pixel_size.height(),
                                                       is_opaque));
  SkScalar scale_scalar = SkFloatToScalar(image_scale);
  canvas_->scale(scale_scalar, scale_scalar);
}

// static
void Canvas::SizeStringInt(const base::string16& text,
                           const FontList& font_list,
                           int* width,
                           int* height,
                           int line_height,
                           int flags) {
  float fractional_width = static_cast<float>(*width);
  float factional_height = static_cast<float>(*height);
  SizeStringFloat(text, font_list, &fractional_width,
                  &factional_height, line_height, flags);
  *width = ToCeiledInt(fractional_width);
  *height = ToCeiledInt(factional_height);
}

// static
int Canvas::GetStringWidth(const base::string16& text,
                           const FontList& font_list) {
  int width = 0, height = 0;
  SizeStringInt(text, font_list, &width, &height, 0, NO_ELLIPSIS);
  return width;
}

// static
float Canvas::GetStringWidthF(const base::string16& text,
                              const FontList& font_list) {
  float width = 0, height = 0;
  SizeStringFloat(text, font_list, &width, &height, 0, NO_ELLIPSIS);
  return width;
}

// static
int Canvas::DefaultCanvasTextAlignment() {
  return base::i18n::IsRTL() ? TEXT_ALIGN_RIGHT : TEXT_ALIGN_LEFT;
}

ImageSkiaRep Canvas::ExtractImageRep() const {
  // Make a bitmap to return, and a canvas to draw into it. We don't just want
  // to call extractSubset or the copy constructor, since we want an actual copy
  // of the bitmap.
  const SkISize size = canvas_->getBaseLayerSize();
  SkBitmap result;
  result.allocN32Pixels(size.width(), size.height());

  canvas_->readPixels(&result, 0, 0);
  return ImageSkiaRep(result, image_scale_);
}

void Canvas::DrawDashedRect(const Rect& rect, SkColor color) {
  if (rect.IsEmpty())
    return;
  // Create a 2D bitmap containing alternating on/off pixels - we do this
  // so that you never get two pixels of the same color around the edges
  // of the focus rect (this may mean that opposing edges of the rect may
  // have a dot pattern out of phase to each other).
  static SkColor last_color;
  static SkBitmap* dots = NULL;
  if (!dots || last_color != color) {
    int col_pixels = 32;
    int row_pixels = 32;

    delete dots;
    last_color = color;
    dots = new SkBitmap;
    dots->allocN32Pixels(col_pixels, row_pixels);
    dots->eraseARGB(0, 0, 0, 0);

    uint32_t* dot = dots->getAddr32(0, 0);
    for (int i = 0; i < row_pixels; i++) {
      for (int u = 0; u < col_pixels; u++) {
        if ((u % 2 + i % 2) % 2 != 0) {
          dot[i * row_pixels + u] = color;
        }
      }
    }
  }

  // Make a shader for the bitmap with an origin of the box we'll draw.
  SkPaint paint;
  paint.setShader(SkShader::MakeBitmapShader(*dots, SkShader::kRepeat_TileMode,
                                             SkShader::kRepeat_TileMode));

  DrawRect(Rect(rect.x(), rect.y(), rect.width(), 1), paint);
  DrawRect(Rect(rect.x(), rect.y() + rect.height() - 1, rect.width(), 1),
           paint);
  DrawRect(Rect(rect.x(), rect.y(), 1, rect.height()), paint);
  DrawRect(Rect(rect.x() + rect.width() - 1, rect.y(), 1, rect.height()),
           paint);
}

float Canvas::UndoDeviceScaleFactor() {
  SkScalar scale_factor = 1.0f / image_scale_;
  canvas_->scale(scale_factor, scale_factor);
  return image_scale_;
}

void Canvas::Save() {
  canvas_->save();
}

void Canvas::SaveLayerAlpha(uint8_t alpha) {
  canvas_->saveLayerAlpha(NULL, alpha);
}

void Canvas::SaveLayerAlpha(uint8_t alpha, const Rect& layer_bounds) {
  SkRect bounds(RectToSkRect(layer_bounds));
  canvas_->saveLayerAlpha(&bounds, alpha);
}

void Canvas::Restore() {
  canvas_->restore();
}

void Canvas::ClipRect(const Rect& rect) {
  canvas_->clipRect(RectToSkRect(rect));
}

void Canvas::ClipPath(const SkPath& path, bool do_anti_alias) {
  canvas_->clipPath(path, SkRegion::kIntersect_Op, do_anti_alias);
}

bool Canvas::IsClipEmpty() const {
  return canvas_->isClipEmpty();
}

bool Canvas::GetClipBounds(Rect* bounds) {
  SkRect out;
  if (canvas_->getClipBounds(&out)) {
    *bounds = ToEnclosingRect(SkRectToRectF(out));
    return true;
  }
  *bounds = gfx::Rect();
  return false;
}

void Canvas::Translate(const Vector2d& offset) {
  canvas_->translate(SkIntToScalar(offset.x()), SkIntToScalar(offset.y()));
}

void Canvas::Scale(int x_scale, int y_scale) {
  canvas_->scale(SkIntToScalar(x_scale), SkIntToScalar(y_scale));
}

void Canvas::DrawColor(SkColor color) {
  DrawColor(color, SkXfermode::kSrcOver_Mode);
}

void Canvas::DrawColor(SkColor color, SkXfermode::Mode mode) {
  canvas_->drawColor(color, mode);
}

void Canvas::FillRect(const Rect& rect, SkColor color) {
  FillRect(rect, color, SkXfermode::kSrcOver_Mode);
}

void Canvas::FillRect(const Rect& rect,
                      SkColor color,
                      SkXfermode::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setXfermodeMode(mode);
  DrawRect(rect, paint);
}

void Canvas::DrawRect(const Rect& rect, SkColor color) {
  DrawRect(rect, color, SkXfermode::kSrcOver_Mode);
}

void Canvas::DrawRect(const Rect& rect,
                      SkColor color,
                      SkXfermode::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kStroke_Style);
  // Set a stroke width of 0, which will put us down the stroke rect path.  If
  // we set a stroke width of 1, for example, this will internally create a
  // path and fill it, which causes problems near the edge of the canvas.
  paint.setStrokeWidth(SkIntToScalar(0));
  paint.setXfermodeMode(mode);

  DrawRect(rect, paint);
}

void Canvas::DrawRect(const Rect& rect, const SkPaint& paint) {
  canvas_->drawIRect(RectToSkIRect(rect), paint);
}

void Canvas::DrawPoint(const Point& p1, const SkPaint& paint) {
  canvas_->drawPoint(SkIntToScalar(p1.x()), SkIntToScalar(p1.y()), paint);
}

void Canvas::DrawLine(const Point& p1, const Point& p2, SkColor color) {
  DrawLine(PointF(p1), PointF(p2), color);
}

void Canvas::DrawLine(const PointF& p1, const PointF& p2, SkColor color) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStrokeWidth(SkIntToScalar(1));
  DrawLine(p1, p2, paint);
}

void Canvas::DrawLine(const Point& p1, const Point& p2, const SkPaint& paint) {
  DrawLine(PointF(p1), PointF(p2), paint);
}

void Canvas::DrawLine(const PointF& p1,
                      const PointF& p2,
                      const SkPaint& paint) {
  canvas_->drawLine(SkFloatToScalar(p1.x()), SkFloatToScalar(p1.y()),
                    SkFloatToScalar(p2.x()), SkFloatToScalar(p2.y()), paint);
}

void Canvas::DrawCircle(const Point& center_point,
                        int radius,
                        const SkPaint& paint) {
  canvas_->drawCircle(SkIntToScalar(center_point.x()),
      SkIntToScalar(center_point.y()), SkIntToScalar(radius), paint);
}

void Canvas::DrawCircle(const PointF& center_point,
                        float radius,
                        const SkPaint& paint) {
  canvas_->drawCircle(SkFloatToScalar(center_point.x()),
                      SkFloatToScalar(center_point.y()), radius, paint);
}

void Canvas::DrawRoundRect(const Rect& rect,
                           int radius,
                           const SkPaint& paint) {
  canvas_->drawRoundRect(RectToSkRect(rect), SkIntToScalar(radius),
                         SkIntToScalar(radius), paint);
}

void Canvas::DrawRoundRect(const RectF& rect,
                           float radius,
                           const SkPaint& paint) {
  canvas_->drawRoundRect(RectFToSkRect(rect), SkFloatToScalar(radius),
                         SkFloatToScalar(radius), paint);
}

void Canvas::DrawPath(const SkPath& path, const SkPaint& paint) {
  canvas_->drawPath(path, paint);
}

void Canvas::DrawFocusRect(const Rect& rect) {
  DrawDashedRect(rect, SK_ColorGRAY);
}

void Canvas::DrawSolidFocusRect(const Rect& rect, SkColor color) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStrokeWidth(SkIntToScalar(1));
  // Note: We cannot use DrawRect since it would create a path and fill it which
  // would cause problems near the edge of the canvas.
  int x1 = std::min(rect.x(), rect.right());
  int x2 = std::max(rect.x(), rect.right());
  int y1 = std::min(rect.y(), rect.bottom());
  int y2 = std::max(rect.y(), rect.bottom());
  DrawLine(Point(x1, y1), Point(x2, y1), paint);
  DrawLine(Point(x1, y2), Point(x2, y2), paint);
  DrawLine(Point(x1, y1), Point(x1, y2), paint);
  DrawLine(Point(x2, y1), Point(x2, y2 + 1), paint);
}

void Canvas::DrawImageInt(const ImageSkia& image, int x, int y) {
  SkPaint paint;
  DrawImageInt(image, x, y, paint);
}

void Canvas::DrawImageInt(const ImageSkia& image, int x, int y, uint8_t a) {
  SkPaint paint;
  paint.setAlpha(a);
  DrawImageInt(image, x, y, paint);
}

void Canvas::DrawImageInt(const ImageSkia& image,
                          int x,
                          int y,
                          const SkPaint& paint) {
  const ImageSkiaRep& image_rep = image.GetRepresentation(image_scale_);
  if (image_rep.is_null())
    return;
  const SkBitmap& bitmap = image_rep.sk_bitmap();
  float bitmap_scale = image_rep.scale();

  ScopedCanvas scoper(this);
  canvas_->scale(SkFloatToScalar(1.0f / bitmap_scale),
                 SkFloatToScalar(1.0f / bitmap_scale));
  canvas_->drawBitmap(bitmap,
                      SkFloatToScalar(x * bitmap_scale),
                      SkFloatToScalar(y * bitmap_scale),
                      &paint);
}

void Canvas::DrawImageInt(const ImageSkia& image,
                          int src_x,
                          int src_y,
                          int src_w,
                          int src_h,
                          int dest_x,
                          int dest_y,
                          int dest_w,
                          int dest_h,
                          bool filter) {
  SkPaint p;
  DrawImageInt(image, src_x, src_y, src_w, src_h, dest_x, dest_y,
               dest_w, dest_h, filter, p);
}

void Canvas::DrawImageInt(const ImageSkia& image,
                          int src_x,
                          int src_y,
                          int src_w,
                          int src_h,
                          int dest_x,
                          int dest_y,
                          int dest_w,
                          int dest_h,
                          bool filter,
                          const SkPaint& paint) {
  const ImageSkiaRep& image_rep = image.GetRepresentation(image_scale_);
  if (image_rep.is_null())
    return;
  bool remove_image_scale = true;
  DrawImageIntHelper(image_rep, src_x, src_y, src_w, src_h, dest_x, dest_y,
                     dest_w, dest_h, filter, paint, remove_image_scale);
}

void Canvas::DrawImageIntInPixel(const ImageSkiaRep& image_rep,
                                 int dest_x,
                                 int dest_y,
                                 int dest_w,
                                 int dest_h,
                                 bool filter,
                                 const SkPaint& paint) {
  int src_x = 0;
  int src_y = 0;
  int src_w = image_rep.pixel_width();
  int src_h = image_rep.pixel_height();
  // Don't remove image scale here, this function is used to draw the
  // (already scaled) |image_rep| at a 1:1 scale with the canvas.
  bool remove_image_scale = false;
  DrawImageIntHelper(image_rep, src_x, src_y, src_w, src_h, dest_x, dest_y,
                     dest_w, dest_h, filter, paint, remove_image_scale);
}

void Canvas::DrawImageInPath(const ImageSkia& image,
                             int x,
                             int y,
                             const SkPath& path,
                             const SkPaint& paint) {
  const ImageSkiaRep& image_rep = image.GetRepresentation(image_scale_);
  if (image_rep.is_null())
    return;

  SkMatrix matrix;
  matrix.setTranslate(SkIntToScalar(x), SkIntToScalar(y));
  SkPaint p(paint);
  p.setShader(CreateImageRepShader(image_rep,
                                   SkShader::kRepeat_TileMode,
                                   matrix));
  canvas_->drawPath(path, p);
}

void Canvas::DrawStringRect(const base::string16& text,
                            const FontList& font_list,
                            SkColor color,
                            const Rect& display_rect) {
  DrawStringRectWithFlags(text, font_list, color, display_rect,
                          DefaultCanvasTextAlignment());
}

void Canvas::DrawStringRectWithFlags(const base::string16& text,
                                     const FontList& font_list,
                                     SkColor color,
                                     const Rect& display_rect,
                                     int flags) {
  DrawStringRectWithShadows(text, font_list, color, display_rect, 0, flags,
                            ShadowValues());
}

void Canvas::TileImageInt(const ImageSkia& image,
                          int x,
                          int y,
                          int w,
                          int h) {
  TileImageInt(image, 0, 0, x, y, w, h);
}

void Canvas::TileImageInt(const ImageSkia& image,
                          int src_x,
                          int src_y,
                          int dest_x,
                          int dest_y,
                          int w,
                          int h) {
  TileImageInt(image, src_x, src_y, 1.0f, 1.0f, dest_x, dest_y, w, h);
}

void Canvas::TileImageInt(const ImageSkia& image,
                          int src_x,
                          int src_y,
                          float tile_scale_x,
                          float tile_scale_y,
                          int dest_x,
                          int dest_y,
                          int w,
                          int h) {
  SkRect dest_rect = { SkIntToScalar(dest_x),
                       SkIntToScalar(dest_y),
                       SkIntToScalar(dest_x + w),
                       SkIntToScalar(dest_y + h) };
  if (!IntersectsClipRect(dest_rect))
    return;

  SkPaint paint;
  if (InitSkPaintForTiling(image, src_x, src_y, tile_scale_x, tile_scale_y,
                           dest_x, dest_y, &paint))
    canvas_->drawRect(dest_rect, paint);
}

bool Canvas::InitSkPaintForTiling(const ImageSkia& image,
                                  int src_x,
                                  int src_y,
                                  float tile_scale_x,
                                  float tile_scale_y,
                                  int dest_x,
                                  int dest_y,
                                  SkPaint* paint) {
  const ImageSkiaRep& image_rep = image.GetRepresentation(image_scale_);
  if (image_rep.is_null())
    return false;

  SkMatrix shader_scale;
  shader_scale.setScale(SkFloatToScalar(tile_scale_x),
                        SkFloatToScalar(tile_scale_y));
  shader_scale.preTranslate(SkIntToScalar(-src_x), SkIntToScalar(-src_y));
  shader_scale.postTranslate(SkIntToScalar(dest_x), SkIntToScalar(dest_y));

  paint->setShader(CreateImageRepShader(image_rep, SkShader::kRepeat_TileMode,
                                        shader_scale));
  paint->setXfermodeMode(SkXfermode::kSrcOver_Mode);
  return true;
}

void Canvas::Transform(const gfx::Transform& transform) {
  canvas_->concat(transform.matrix());
}

bool Canvas::IntersectsClipRect(const SkRect& rect) {
  SkRect clip;
  return canvas_->getClipBounds(&clip) && clip.intersects(rect);
}

void Canvas::DrawImageIntHelper(const ImageSkiaRep& image_rep,
                                int src_x,
                                int src_y,
                                int src_w,
                                int src_h,
                                int dest_x,
                                int dest_y,
                                int dest_w,
                                int dest_h,
                                bool filter,
                                const SkPaint& paint,
                                bool remove_image_scale) {
  DLOG_ASSERT(src_x + src_w < std::numeric_limits<int16_t>::max() &&
              src_y + src_h < std::numeric_limits<int16_t>::max());
  if (src_w <= 0 || src_h <= 0) {
    NOTREACHED() << "Attempting to draw bitmap from an empty rect!";
    return;
  }

  SkRect dest_rect = { SkIntToScalar(dest_x),
                       SkIntToScalar(dest_y),
                       SkIntToScalar(dest_x + dest_w),
                       SkIntToScalar(dest_y + dest_h) };
  if (!IntersectsClipRect(dest_rect))
    return;

  float user_scale_x = static_cast<float>(dest_w) / src_w;
  float user_scale_y = static_cast<float>(dest_h) / src_h;

  // Make a bitmap shader that contains the bitmap we want to draw. This is
  // basically what SkCanvas.drawBitmap does internally, but it gives us
  // more control over quality and will use the mipmap in the source image if
  // it has one, whereas drawBitmap won't.
  SkMatrix shader_scale;
  shader_scale.setScale(SkFloatToScalar(user_scale_x),
                        SkFloatToScalar(user_scale_y));
  shader_scale.preTranslate(SkIntToScalar(-src_x), SkIntToScalar(-src_y));
  shader_scale.postTranslate(SkIntToScalar(dest_x), SkIntToScalar(dest_y));

  SkPaint p(paint);
  p.setFilterQuality(filter ? kLow_SkFilterQuality : kNone_SkFilterQuality);
  p.setShader(CreateImageRepShaderForScale(
      image_rep, SkShader::kRepeat_TileMode, shader_scale,
      remove_image_scale ? image_rep.scale() : 1.f));

  // The rect will be filled by the bitmap.
  canvas_->drawRect(dest_rect, p);
}

}  // namespace gfx
