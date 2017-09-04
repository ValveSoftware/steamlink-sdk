// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_cache.h"

#include "cc/playback/display_item_list.h"
#include "ui/compositor/paint_context.h"

namespace ui {

PaintCache::PaintCache() : has_cache_(false) {
}

PaintCache::~PaintCache() {
}

bool PaintCache::UseCache(const PaintContext& context,
                          const gfx::Size& size_in_context) {
  if (!has_cache_)
    return false;
  DCHECK(context.list_);
  gfx::Rect bounds_in_layer = context.ToLayerSpaceBounds(size_in_context);
  context.list_->CreateAndAppendDrawingItem<cc::DrawingDisplayItem>(
      bounds_in_layer, display_item_);
  return true;
}

void PaintCache::SetCache(const cc::DrawingDisplayItem& item) {
  item.CloneTo(&display_item_);
  has_cache_ = true;
}

}  // namespace ui
