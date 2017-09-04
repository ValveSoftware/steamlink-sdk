// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRRect.h"
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
    SkColor fill_color,
    int corner_radius)
    : BasePaintedLayerDelegate(gfx::kPlaceholderColor),
      shadows_(shadows),
      bounds_(shadowed_area_bounds),
      fill_color_(fill_color),
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
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setColor(fill_color_);

  gfx::RectF rrect_bounds =
      gfx::RectF(bounds_ - GetPaintedBounds().OffsetFromOrigin());
  SkRRect r_rect = SkRRect::MakeRectXY(gfx::RectFToSkRect(rrect_bounds),
                                       corner_radius_, corner_radius_);

  // First the fill color.
  ui::PaintRecorder recorder(context, GetPaintedBounds().size());
  recorder.canvas()->sk_canvas()->drawRRect(r_rect, paint);

  // Now the shadow.
  paint.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadows_));
  recorder.canvas()->sk_canvas()->clipRRect(r_rect, SkRegion::kDifference_Op,
                                            true);
  recorder.canvas()->sk_canvas()->drawRRect(r_rect, paint);
}

}  // namespace views
