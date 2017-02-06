// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// BasePaintedLayerDelegate
//

BasePaintedLayerDelegate::BasePaintedLayerDelegate(SkColor color)
    : color_(color) {}

BasePaintedLayerDelegate::~BasePaintedLayerDelegate() {}

gfx::Vector2dF BasePaintedLayerDelegate::GetCenteringOffset() const {
  return gfx::RectF(GetPaintedBounds()).CenterPoint().OffsetFromOrigin();
}

void BasePaintedLayerDelegate::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {}

void BasePaintedLayerDelegate::OnDeviceScaleFactorChanged(
    float device_scale_factor) {}

base::Closure BasePaintedLayerDelegate::PrepareForLayerBoundsChange() {
  return base::Closure();
}

////////////////////////////////////////////////////////////////////////////////
//
// CircleLayerDelegate
//

CircleLayerDelegate::CircleLayerDelegate(SkColor color, int radius)
    : BasePaintedLayerDelegate(color), radius_(radius) {}

CircleLayerDelegate::~CircleLayerDelegate() {}

gfx::Rect CircleLayerDelegate::GetPaintedBounds() const {
  const int diameter = radius_ * 2;
  return gfx::Rect(0, 0, diameter, diameter);
}

void CircleLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color());
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  ui::PaintRecorder recorder(context, GetPaintedBounds().size());
  gfx::Canvas* canvas = recorder.canvas();

  canvas->DrawCircle(GetPaintedBounds().CenterPoint(), radius_, paint);
}

////////////////////////////////////////////////////////////////////////////////
//
// RectangleLayerDelegate
//

RectangleLayerDelegate::RectangleLayerDelegate(SkColor color, gfx::Size size)
    : BasePaintedLayerDelegate(color), size_(size) {}

RectangleLayerDelegate::~RectangleLayerDelegate() {}

gfx::Rect RectangleLayerDelegate::GetPaintedBounds() const {
  return gfx::Rect(size_);
}

void RectangleLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color());
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  ui::PaintRecorder recorder(context, size_);
  gfx::Canvas* canvas = recorder.canvas();
  canvas->DrawRect(GetPaintedBounds(), paint);
}

////////////////////////////////////////////////////////////////////////////////
//
// RoundedRectangleLayerDelegate
//

RoundedRectangleLayerDelegate::RoundedRectangleLayerDelegate(
    SkColor color,
    const gfx::Size& size,
    int corner_radius)
    : BasePaintedLayerDelegate(color),
      size_(size),
      corner_radius_(corner_radius) {}

RoundedRectangleLayerDelegate::~RoundedRectangleLayerDelegate() {}

gfx::Rect RoundedRectangleLayerDelegate::GetPaintedBounds() const {
  return gfx::Rect(size_);
}

void RoundedRectangleLayerDelegate::OnPaintLayer(
    const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color());
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  ui::PaintRecorder recorder(context, size_);
  recorder.canvas()->DrawRoundRect(GetPaintedBounds(), corner_radius_, paint);
}

////////////////////////////////////////////////////////////////////////////////
//
// BorderShadowLayerDelegate
//

BorderShadowLayerDelegate::BorderShadowLayerDelegate(
    const std::vector<gfx::ShadowValue>& shadows,
    const gfx::Rect& shadowed_area_bounds,
    int corner_radius)
    : BasePaintedLayerDelegate(gfx::kPlaceholderColor),
      shadows_(shadows),
      bounds_(shadowed_area_bounds),
      corner_radius_(corner_radius) {}

BorderShadowLayerDelegate::~BorderShadowLayerDelegate() {}

gfx::Rect BorderShadowLayerDelegate::GetPaintedBounds() const {
  gfx::Rect total_rect(bounds_);
  total_rect.Inset(gfx::ShadowValue::GetMargin(shadows_));
  return total_rect;
}

gfx::Vector2dF BorderShadowLayerDelegate::GetCenteringOffset() const {
  return gfx::RectF(bounds_).CenterPoint().OffsetFromOrigin();
}

void BorderShadowLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadows_));
  paint.setStyle(SkPaint::kStrokeAndFill_Style);
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorTRANSPARENT);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  SkRect path_bounds = gfx::RectFToSkRect(
      gfx::RectF(bounds_ - GetPaintedBounds().OffsetFromOrigin()));

  ui::PaintRecorder recorder(context, GetPaintedBounds().size());
  const SkScalar corner = SkFloatToScalar(corner_radius_);
  SkPath path;
  path.addRoundRect(path_bounds, corner, corner, SkPath::kCCW_Direction);
  recorder.canvas()->DrawPath(path, paint);

  SkPaint clear_paint;
  clear_paint.setAntiAlias(true);
  clear_paint.setXfermodeMode(SkXfermode::kClear_Mode);
  recorder.canvas()->sk_canvas()->drawRoundRect(path_bounds, corner, corner,
                                                clear_paint);
}

}  // namespace views
