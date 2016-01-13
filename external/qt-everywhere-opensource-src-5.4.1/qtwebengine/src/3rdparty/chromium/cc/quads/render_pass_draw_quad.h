// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_RENDER_PASS_DRAW_QUAD_H_
#define CC_QUADS_RENDER_PASS_DRAW_QUAD_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/resource_provider.h"

namespace cc {

class CC_EXPORT RenderPassDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<RenderPassDrawQuad> Create();
  virtual ~RenderPassDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              RenderPass::Id render_pass_id,
              bool is_replica,
              ResourceProvider::ResourceId mask_resource_id,
              const gfx::Rect& contents_changed_since_last_frame,
              const gfx::RectF& mask_uv_rect,
              const FilterOperations& filters,
              const FilterOperations& background_filters);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              RenderPass::Id render_pass_id,
              bool is_replica,
              ResourceProvider::ResourceId mask_resource_id,
              const gfx::Rect& contents_changed_since_last_frame,
              const gfx::RectF& mask_uv_rect,
              const FilterOperations& filters,
              const FilterOperations& background_filters);

  scoped_ptr<RenderPassDrawQuad> Copy(
      const SharedQuadState* copied_shared_quad_state,
      RenderPass::Id copied_render_pass_id) const;

  RenderPass::Id render_pass_id;
  bool is_replica;
  ResourceProvider::ResourceId mask_resource_id;
  gfx::Rect contents_changed_since_last_frame;
  gfx::RectF mask_uv_rect;

  // Post-processing filters, applied to the pixels in the render pass' texture.
  FilterOperations filters;

  // Post-processing filters, applied to the pixels showing through the
  // background of the render pass, from behind it.
  FilterOperations background_filters;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const RenderPassDrawQuad* MaterialCast(const DrawQuad*);

 private:
  RenderPassDrawQuad();
  virtual void ExtendValue(base::DictionaryValue* value) const OVERRIDE;
};

}  // namespace cc

#endif  // CC_QUADS_RENDER_PASS_DRAW_QUAD_H_
