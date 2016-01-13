// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_TILE_DRAW_QUAD_H_
#define CC_QUADS_TILE_DRAW_QUAD_H_

#include "cc/quads/content_draw_quad_base.h"

namespace cc {

class CC_EXPORT TileDrawQuad : public ContentDrawQuadBase {
 public:
  static scoped_ptr<TileDrawQuad> Create();
  virtual ~TileDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              unsigned resource_id,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool swizzle_contents);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              unsigned resource_id,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool swizzle_contents);

  unsigned resource_id;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const TileDrawQuad* MaterialCast(const DrawQuad*);

 private:
  TileDrawQuad();
  virtual void ExtendValue(base::DictionaryValue* value) const OVERRIDE;
};

}  // namespace cc

#endif  // CC_QUADS_TILE_DRAW_QUAD_H_
