// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/fallback_icon_service.h"

#include <stddef.h>

#include <algorithm>

#include "components/favicon/core/fallback_icon_client.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace favicon {
namespace {

// Arbitrary maximum icon size, can be reasonably increased if needed.
const int kMaxFallbackFaviconSize = 288;

}  // namespace

FallbackIconService::FallbackIconService(
    FallbackIconClient* fallback_icon_client)
    : fallback_icon_client_(fallback_icon_client) {
}

FallbackIconService::~FallbackIconService() {
}

std::vector<unsigned char> FallbackIconService::RenderFallbackIconBitmap(
    const GURL& icon_url,
    int size,
    const favicon_base::FallbackIconStyle& style) {
  int size_to_use = std::min(kMaxFallbackFaviconSize, size);
  gfx::Canvas canvas(gfx::Size(size_to_use, size_to_use), 1.0f, false);
  DrawFallbackIcon(icon_url, size_to_use, style, &canvas);

  std::vector<unsigned char> bitmap_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(canvas.ExtractImageRep().sk_bitmap(),
                                         false, &bitmap_data)) {
    bitmap_data.clear();
  }
  return bitmap_data;
}

void FallbackIconService::DrawFallbackIcon(
    const GURL& icon_url,
    int size,
    const favicon_base::FallbackIconStyle& style,
    gfx::Canvas* canvas) {
  const int kOffsetX = 0;
  const int kOffsetY = 0;
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  // Draw a filled, colored rounded square.
  paint.setColor(style.background_color);
  int corner_radius = static_cast<int>(size * style.roundness * 0.5 + 0.5);
  canvas->DrawRoundRect(
      gfx::Rect(kOffsetX, kOffsetY, size, size), corner_radius, paint);

  // Draw text.
  base::string16 icon_text = GetFallbackIconText(icon_url);
  if (icon_text.empty())
    return;
  int font_size = static_cast<int>(size * style.font_size_ratio);
  if (font_size <= 0)
    return;

  canvas->DrawStringRectWithFlags(
      icon_text,
      gfx::FontList(fallback_icon_client_->GetFontNameList(), gfx::Font::NORMAL,
                    font_size, gfx::Font::Weight::NORMAL),
      style.text_color, gfx::Rect(kOffsetX, kOffsetY, size, size),
      gfx::Canvas::TEXT_ALIGN_CENTER);
}

}  // namespace favicon
