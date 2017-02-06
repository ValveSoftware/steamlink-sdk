// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_CRUSHED_SPRITE_RESOURCE_H_
#define UI_ANDROID_RESOURCES_CRUSHED_SPRITE_RESOURCE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class LayerTreeHost;
class ScopedUIResource;
typedef int UIResourceId;
}

namespace gfx {
class JavaBitmap;
}

namespace ui {

// A resource that provides an unscaled bitmap and corresponding metadata for a
// crushed sprite. A crushed sprite animation is run by drawing rectangles from
// a bitmap to a canvas. Each frame in the animation draws its rectangles on top
// of the previous frame.
class UI_ANDROID_EXPORT CrushedSpriteResource {
 public:
  typedef std::vector<std::pair<gfx::Rect, gfx::Rect>> FrameSrcDstRects;
  typedef std::vector<FrameSrcDstRects> SrcDstRects;

  // Creates a new CrushedSpriteResource. |bitmap_res_id| is the id for the
  // for the source bitmap, |java_bitmap| is the source bitmap for the crushed
  // sprite, |src_dst_rects| is a list of rectangles to draw for each frame, and
  // |unscaled_sprite_size| is the size of an individual sprite unscaled. The
  // sprite should be drawn at its unscaled size then scaled to
  // |scaled_sprite_size|.
  CrushedSpriteResource(const SkBitmap& bitmap,
                        const SrcDstRects& src_dst_rects,
                        gfx::Size unscaled_sprite_size,
                        gfx::Size scaled_sprite_size);
  ~CrushedSpriteResource();

  // Sets the source bitmap.
  void SetBitmap(const SkBitmap& bitmap);

  // Returns the source bitmap.
  const SkBitmap& GetBitmap();

  // Evicts bitmap_ from memory.
  void EvictBitmapFromMemory();

  // Returns true if the source bitmap has been evicted from memory.
  bool BitmapHasBeenEvictedFromMemory();

  // Returns a list of rectangles to be drawn for |frame|.
  FrameSrcDstRects GetRectanglesForFrame(int frame);

  // Returns the unscaled size of an individual sprite.
  gfx::Size GetUnscaledSpriteSize();

  // Returns the scaled size of an individual sprite.
  gfx::Size GetScaledSpriteSize();

  // Returns the total number of frames in the sprite animation.
  int GetFrameCount();

 private:
  SkBitmap bitmap_;
  std::unique_ptr<cc::ScopedUIResource> last_frame_resource_;
  SrcDstRects src_dst_rects_;
  gfx::Size unscaled_sprite_size_;
  gfx::Size scaled_sprite_size_;

  DISALLOW_COPY_AND_ASSIGN(CrushedSpriteResource);
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_CRUSHED_SPRITE_RESOURCE_H_
