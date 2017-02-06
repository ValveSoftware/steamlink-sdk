// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/canvas_painter.h"

#include "cc/playback/display_item_list.h"
#include "cc/playback/display_item_list_settings.h"
#include "ui/gfx/canvas.h"

namespace ui {

CanvasPainter::CanvasPainter(gfx::Canvas* canvas, float raster_scale_factor)
    : canvas_(canvas),
      raster_scale_factor_(raster_scale_factor),
      rect_(gfx::ScaleToEnclosedRect(
          gfx::Rect(canvas_->sk_canvas()->getBaseLayerSize().width(),
                    canvas_->sk_canvas()->getBaseLayerSize().height()),
          1.f / raster_scale_factor)),
      list_(cc::DisplayItemList::Create(rect_, cc::DisplayItemListSettings())),
      context_(list_.get(), raster_scale_factor_, rect_) {
}

CanvasPainter::~CanvasPainter() {
  list_->Finalize();
  list_->Raster(canvas_->sk_canvas(), nullptr, rect_, raster_scale_factor_);
}

}  // namespace ui
