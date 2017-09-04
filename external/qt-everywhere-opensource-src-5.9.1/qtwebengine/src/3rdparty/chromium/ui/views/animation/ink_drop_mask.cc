// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_mask.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

namespace views {

// InkDropMask

InkDropMask::InkDropMask(const gfx::Size& layer_size)
    : layer_(ui::LAYER_TEXTURED) {
  layer_.set_delegate(this);
  layer_.SetBounds(gfx::Rect(layer_size));
  layer_.SetFillsBoundsOpaquely(false);
  layer_.set_name("InkDropMaskLayer");
}

InkDropMask::~InkDropMask() {
  layer_.set_delegate(nullptr);
}

void InkDropMask::UpdateLayerSize(const gfx::Size& new_layer_size) {
  layer_.SetBounds(gfx::Rect(new_layer_size));
}

void InkDropMask::OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) {}

void InkDropMask::OnDeviceScaleFactorChanged(float device_scale_factor) {}

// RoundRectInkDropMask

RoundRectInkDropMask::RoundRectInkDropMask(const gfx::Size& layer_size,
                                           const gfx::Insets& mask_insets,
                                           int corner_radius)
    : InkDropMask(layer_size),
      mask_insets_(mask_insets),
      corner_radius_(corner_radius) {}

void RoundRectInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setAlpha(255);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Rect bounds = layer()->bounds();
  bounds.Inset(mask_insets_);
  recorder.canvas()->DrawRoundRect(bounds, corner_radius_, paint);
}

// CircleInkDropMask

CircleInkDropMask::CircleInkDropMask(const gfx::Size& layer_size,
                                     const gfx::Point& mask_center,
                                     int mask_radius)
    : InkDropMask(layer_size),
      mask_center_(mask_center),
      mask_radius_(mask_radius) {}

void CircleInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setAlpha(255);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  ui::PaintRecorder recorder(context, layer()->size());
  recorder.canvas()->DrawCircle(mask_center_, mask_radius_, paint);
}

}  // namespace views
