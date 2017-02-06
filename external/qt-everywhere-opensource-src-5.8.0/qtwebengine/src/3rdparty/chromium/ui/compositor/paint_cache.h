// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PAINT_CACHE_H_
#define UI_COMPOSITOR_PAINT_CACHE_H_

#include "base/macros.h"
#include "cc/playback/drawing_display_item.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class PaintContext;
class PaintRecorder;

// A class that holds the output of a PaintRecorder to be reused when the
// object that created the PaintRecorder has not been changed/invalidated.
class COMPOSITOR_EXPORT PaintCache {
 public:
  PaintCache();
  ~PaintCache();

  // Returns true if the PaintCache was able to insert a previously-saved
  // painting output into the PaintContext. If it returns false, the caller
  // needs to do the work of painting, which can be stored into the PaintCache
  // to be used next time.
  bool UseCache(const PaintContext& context, const gfx::Size& size_in_context);

 private:
  // Only PaintRecorder can modify these.
  friend PaintRecorder;

  void SetCache(const cc::DrawingDisplayItem& item);

  bool has_cache_;
  cc::DrawingDisplayItem display_item_;

  DISALLOW_COPY_AND_ASSIGN(PaintCache);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_PAINT_CACHE_H_
