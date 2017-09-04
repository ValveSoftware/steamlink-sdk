// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/crushed_sprite_resource.h"

#include "cc/trees/layer_tree_host.h"
#include "ui/gfx/android/java_bitmap.h"

namespace ui {

CrushedSpriteResource::CrushedSpriteResource(
    const SkBitmap& bitmap,
    const SrcDstRects& src_dst_rects,
    gfx::Size unscaled_sprite_size,
    gfx::Size scaled_sprite_size)
    : src_dst_rects_(src_dst_rects),
      unscaled_sprite_size_(unscaled_sprite_size),
      scaled_sprite_size_(scaled_sprite_size) {
  SetBitmap(bitmap);
}

CrushedSpriteResource::~CrushedSpriteResource() {
}

void CrushedSpriteResource::SetBitmap(const SkBitmap& bitmap) {
  bitmap_ = bitmap;
  bitmap_.setImmutable();
}

const SkBitmap& CrushedSpriteResource::GetBitmap() {
  DCHECK(!bitmap_.empty());
  return bitmap_;
}

void CrushedSpriteResource::EvictBitmapFromMemory() {
  bitmap_.reset();
}

bool CrushedSpriteResource::BitmapHasBeenEvictedFromMemory() {
  return bitmap_.empty();
}

CrushedSpriteResource::FrameSrcDstRects
CrushedSpriteResource::GetRectanglesForFrame(int frame) {
  DCHECK(frame >= 0 && frame < static_cast<int>(src_dst_rects_.size()));
  return src_dst_rects_[frame];
}

gfx::Size CrushedSpriteResource::GetUnscaledSpriteSize() {
  return unscaled_sprite_size_;
}

gfx::Size CrushedSpriteResource::GetScaledSpriteSize() {
  return scaled_sprite_size_;
}

int CrushedSpriteResource::GetFrameCount() {
  return src_dst_rects_.size();
}

size_t CrushedSpriteResource::EstimateMemoryUsage() const {
  return bitmap_.getSize();
}

}  // namespace ui
