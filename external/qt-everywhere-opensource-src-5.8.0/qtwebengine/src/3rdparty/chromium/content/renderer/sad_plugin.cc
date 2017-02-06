// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/sad_plugin.h"

#include <algorithm>
#include <memory>

#include "skia/ext/platform_canvas.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

void PaintSadPlugin(blink::WebCanvas* webcanvas,
                    const gfx::Rect& plugin_rect,
                    const SkBitmap& sad_plugin_bitmap) {
  const int width = plugin_rect.width();
  const int height = plugin_rect.height();

  SkCanvas* canvas = webcanvas;
  SkAutoCanvasRestore auto_restore(canvas, true);
  // We draw the sad-plugin bitmap at the origin of canvas.
  // Add a translation so that it appears at the origin of plugin rect.
  canvas->translate(plugin_rect.x(), plugin_rect.y());

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);
  canvas->drawRectCoords(0, 0, SkIntToScalar(width), SkIntToScalar(height),
                         paint);
  canvas->drawBitmap(
      sad_plugin_bitmap,
      SkIntToScalar(std::max(0, (width - sad_plugin_bitmap.width()) / 2)),
      SkIntToScalar(std::max(0, (height - sad_plugin_bitmap.height()) / 2)));
}

}  // namespace content
