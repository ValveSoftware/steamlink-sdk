// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CANVAS_PAINTER_H_
#define UI_COMPOSITOR_CANVAS_PAINTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class DisplayItemList;
}

namespace gfx {
class Canvas;
}

namespace ui {

// This class provides a simple helper for painting directly to a bitmap
// backed canvas (for painting use-cases other than compositing). After
// constructing an instance of a CanvasPainter, the context() can be used
// to do painting using the normal composited paint paths. When
// the painter is destroyed, any painting done with the context() will be
// rastered into the canvas.
class COMPOSITOR_EXPORT CanvasPainter {
 public:
  CanvasPainter(gfx::Canvas* canvas, float raster_scale_factor);
  ~CanvasPainter();

  const PaintContext& context() const { return context_; }

 private:
  gfx::Canvas* const canvas_;
  const float raster_scale_factor_;
  const gfx::Rect rect_;
  scoped_refptr<cc::DisplayItemList> list_;
  PaintContext context_;

  DISALLOW_COPY_AND_ASSIGN(CanvasPainter);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CANVAS_PAINTER_H_
