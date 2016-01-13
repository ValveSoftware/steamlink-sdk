// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_YUV_VIDEO_DRAW_QUAD_H_
#define CC_QUADS_YUV_VIDEO_DRAW_QUAD_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/quads/draw_quad.h"

namespace cc {

class CC_EXPORT YUVVideoDrawQuad : public DrawQuad {
 public:
  enum ColorSpace {
    REC_601,       // SDTV standard with restricted "studio swing" color range.
    REC_601_JPEG,  // Full color range [0, 255] variant of the above.
    COLOR_SPACE_LAST = REC_601_JPEG
  };

  virtual ~YUVVideoDrawQuad();

  static scoped_ptr<YUVVideoDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              const gfx::RectF& tex_coord_rect,
              unsigned y_plane_resource_id,
              unsigned u_plane_resource_id,
              unsigned v_plane_resource_id,
              unsigned a_plane_resource_id,
              ColorSpace color_space);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              unsigned y_plane_resource_id,
              unsigned u_plane_resource_id,
              unsigned v_plane_resource_id,
              unsigned a_plane_resource_id,
              ColorSpace color_space);

  gfx::RectF tex_coord_rect;
  unsigned y_plane_resource_id;
  unsigned u_plane_resource_id;
  unsigned v_plane_resource_id;
  unsigned a_plane_resource_id;
  ColorSpace color_space;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const YUVVideoDrawQuad* MaterialCast(const DrawQuad*);

 private:
  YUVVideoDrawQuad();
  virtual void ExtendValue(base::DictionaryValue* value) const OVERRIDE;
};

}  // namespace cc

#endif  // CC_QUADS_YUV_VIDEO_DRAW_QUAD_H_
