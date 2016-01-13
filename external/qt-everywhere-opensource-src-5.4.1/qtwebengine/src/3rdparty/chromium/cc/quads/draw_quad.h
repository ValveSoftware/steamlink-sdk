// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_DRAW_QUAD_H_
#define CC_QUADS_DRAW_QUAD_H_

#include "base/callback.h"
#include "cc/base/cc_export.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/resources/resource_provider.h"

namespace base {
class Value;
class DictionaryValue;
}

namespace cc {

// DrawQuad is a bag of data used for drawing a quad. Because different
// materials need different bits of per-quad data to render, classes that derive
// from DrawQuad store additional data in their derived instance. The Material
// enum is used to "safely" downcast to the derived class.
// Note: quads contain rects and sizes, which live in different spaces. There is
// the "content space", which is the arbitrary space in which the quad's
// geometry is defined (generally related to the layer that produced the quad,
// e.g. the content space for TiledLayerImpls, or the geometry space for
// PictureLayerImpls). There is also the "target space", which is the space, in
// "physical" pixels, of the render target where the quads is drawn. The quad's
// transform maps the content space to the target space.
class CC_EXPORT DrawQuad {
 public:
  enum Material {
    INVALID,
    CHECKERBOARD,
    DEBUG_BORDER,
    IO_SURFACE_CONTENT,
    PICTURE_CONTENT,
    RENDER_PASS,
    SOLID_COLOR,
    STREAM_VIDEO_CONTENT,
    SURFACE_CONTENT,
    TEXTURE_CONTENT,
    TILED_CONTENT,
    YUV_VIDEO_CONTENT,
    MATERIAL_LAST = YUV_VIDEO_CONTENT
  };

  virtual ~DrawQuad();

  scoped_ptr<DrawQuad> Copy(
      const SharedQuadState* copied_shared_quad_state) const;

  // TODO(danakj): Chromify or remove these SharedQuadState helpers.
  const gfx::Transform& quadTransform() const {
    return shared_quad_state->content_to_target_transform;
  }
  gfx::Rect visibleContentRect() const {
    return shared_quad_state->visible_content_rect;
  }
  gfx::Rect clipRect() const { return shared_quad_state->clip_rect; }
  bool isClipped() const { return shared_quad_state->is_clipped; }
  float opacity() const { return shared_quad_state->opacity; }

  Material material;

  // This rect, after applying the quad_transform(), gives the geometry that
  // this quad should draw to. This rect lives in content space.
  gfx::Rect rect;

  // This specifies the region of the quad that is opaque. This rect lives in
  // content space.
  gfx::Rect opaque_rect;

  // Allows changing the rect that gets drawn to make it smaller. This value
  // should be clipped to |rect|. This rect lives in content space.
  gfx::Rect visible_rect;

  // By default blending is used when some part of the quad is not opaque.
  // With this setting, it is possible to force blending on regardless of the
  // opaque area.
  bool needs_blending;

  // Stores state common to a large bundle of quads; kept separate for memory
  // efficiency. There is special treatment to reconstruct these pointers
  // during serialization.
  const SharedQuadState* shared_quad_state;

  bool IsDebugQuad() const { return material == DEBUG_BORDER; }

  bool ShouldDrawWithBlending() const {
    if (needs_blending || shared_quad_state->opacity < 1.0f)
      return true;
    if (visible_rect.IsEmpty())
      return false;
    return !opaque_rect.Contains(visible_rect);
  }

  typedef ResourceProvider::ResourceId ResourceId;
  typedef base::Callback<ResourceId(ResourceId)> ResourceIteratorCallback;
  virtual void IterateResources(const ResourceIteratorCallback& callback) = 0;

  // Is the left edge of this tile aligned with the originating layer's
  // left edge?
  bool IsLeftEdge() const { return !rect.x(); }

  // Is the top edge of this tile aligned with the originating layer's
  // top edge?
  bool IsTopEdge() const { return !rect.y(); }

  // Is the right edge of this tile aligned with the originating layer's
  // right edge?
  bool IsRightEdge() const {
    return rect.right() == shared_quad_state->content_bounds.width();
  }

  // Is the bottom edge of this tile aligned with the originating layer's
  // bottom edge?
  bool IsBottomEdge() const {
    return rect.bottom() == shared_quad_state->content_bounds.height();
  }

  // Is any edge of this tile aligned with the originating layer's
  // corresponding edge?
  bool IsEdge() const {
    return IsLeftEdge() || IsTopEdge() || IsRightEdge() || IsBottomEdge();
  }

  scoped_ptr<base::Value> AsValue() const;

 protected:
  DrawQuad();

  void SetAll(const SharedQuadState* shared_quad_state,
              Material material,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending);
  virtual void ExtendValue(base::DictionaryValue* value) const = 0;
};

}  // namespace cc

#endif  // CC_QUADS_DRAW_QUAD_H_
