// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/mac/combobox_background_mac.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/view.h"

using ui::NativeThemeMac;

namespace views {

ComboboxBackgroundMac::ComboboxBackgroundMac(int container_width)
    : container_width_(container_width) {}

ComboboxBackgroundMac::~ComboboxBackgroundMac() {}

void ComboboxBackgroundMac::Paint(gfx::Canvas* canvas, View* view) const {
  gfx::RectF bounds(view->GetLocalBounds());
  gfx::ScopedRTLFlipCanvas scoped_canvas(canvas, view->bounds());

  // Inset the left side far enough to draw only the arrow button, and inset the
  // other three sides by half a pixel so the edge of the background doesn't
  // paint outside the border.
  // Disabled comboboxes do not have a separate color for the arrow container;
  // instead, the entire combobox is drawn with the disabled background shader.
  if (view->enabled()) {
    bounds.Inset(bounds.width() - container_width_, 0.5, 0.5, 0.5);
  } else {
    bounds.Inset(0.5, 0.5);
  }

  // TODO(tapted): Check whether the Widget is active, and use the NORMAL
  // BackgroundType if it is inactive. Handling this properly also requires the
  // control to observe the Widget for activation changes and invalidate.
  NativeThemeMac::ButtonBackgroundType type =
      NativeThemeMac::ButtonBackgroundType::HIGHLIGHTED;
  if (!view->enabled())
    type = NativeThemeMac::ButtonBackgroundType::DISABLED;

  SkPaint paint;
  paint.setShader(
      NativeThemeMac::GetButtonBackgroundShader(type, bounds.height()));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  SkPoint no_curve = SkPoint::Make(0, 0);
  SkPoint curve = SkPoint::Make(
      ui::NativeThemeMac::kButtonCornerRadius,
      ui::NativeThemeMac::kButtonCornerRadius);
  SkVector curves[4] = { no_curve, curve, curve, no_curve };

  SkRRect fill_rect;
  if (view->enabled())
    fill_rect.setRectRadii(gfx::RectFToSkRect(bounds), curves);
  else
    fill_rect.setRectXY(gfx::RectFToSkRect(bounds), curve.fX, curve.fY);

  SkPath path;
  path.addRRect(fill_rect);

  canvas->DrawPath(path, paint);
}

}  // namespace views
