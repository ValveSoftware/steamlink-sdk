// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/shadow_border.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"

namespace views {

ShadowBorder::ShadowBorder(int blur,
                           SkColor color,
                           int vertical_offset,
                           int horizontal_offset)
    : views::Border(),
      blur_(blur),
      color_(color),
      vertical_offset_(vertical_offset),
      horizontal_offset_(horizontal_offset) {}

ShadowBorder::~ShadowBorder() {}

// TODO(sidharthms): Re-painting a shadow looper on every paint call may yield
// poor performance. Ideally we should be caching the border to bitmaps.
void ShadowBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  SkPaint paint;
  std::vector<gfx::ShadowValue> shadows;
  shadows.push_back(gfx::ShadowValue(gfx::Point(), blur_, color_));
  skia::RefPtr<SkDrawLooper> looper = gfx::CreateShadowDrawLooper(shadows);
  paint.setLooper(looper.get());
  paint.setColor(SK_ColorTRANSPARENT);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  gfx::Rect bounds(view.size());
  // TODO(pkasting): This isn't right if one of the offsets is larger than
  // (blur_ / 2).
  bounds.Inset(gfx::Insets(blur_ / 2, blur_ / 2, blur_ / 2, blur_ / 2));
  canvas->DrawRect(bounds, paint);
}

gfx::Insets ShadowBorder::GetInsets() const {
  return gfx::Insets(blur_ / 2 - vertical_offset_,
                     blur_ / 2 - horizontal_offset_,
                     blur_ / 2 + vertical_offset_,
                     blur_ / 2 + horizontal_offset_);
}

gfx::Size ShadowBorder::GetMinimumSize() const {
  return gfx::Size(blur_, blur_);
}

}  // namespace views
