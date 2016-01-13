// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass_draw_quad.h"

#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace cc {

RenderPassDrawQuad::RenderPassDrawQuad()
    : render_pass_id(RenderPass::Id(-1, -1)),
      is_replica(false),
      mask_resource_id(-1) {
}

RenderPassDrawQuad::~RenderPassDrawQuad() {
}

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::Create() {
  return make_scoped_ptr(new RenderPassDrawQuad);
}

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::Copy(
    const SharedQuadState* copied_shared_quad_state,
    RenderPass::Id copied_render_pass_id) const {
  scoped_ptr<RenderPassDrawQuad> copy_quad(
      new RenderPassDrawQuad(*MaterialCast(this)));
  copy_quad->shared_quad_state = copied_shared_quad_state;
  copy_quad->render_pass_id = copied_render_pass_id;
  return copy_quad.Pass();
}

void RenderPassDrawQuad::SetNew(
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    RenderPass::Id render_pass_id,
    bool is_replica,
    ResourceProvider::ResourceId mask_resource_id,
    const gfx::Rect& contents_changed_since_last_frame,
    const gfx::RectF& mask_uv_rect,
    const FilterOperations& filters,
    const FilterOperations& background_filters) {
  DCHECK_GT(render_pass_id.layer_id, 0);
  DCHECK_GE(render_pass_id.index, 0);

  gfx::Rect opaque_rect;
  bool needs_blending = false;
  SetAll(shared_quad_state, rect, opaque_rect, visible_rect, needs_blending,
         render_pass_id, is_replica, mask_resource_id,
         contents_changed_since_last_frame, mask_uv_rect, filters,
         background_filters);
}

void RenderPassDrawQuad::SetAll(
    const SharedQuadState* shared_quad_state,
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
    const FilterOperations& background_filters) {
  DCHECK_GT(render_pass_id.layer_id, 0);
  DCHECK_GE(render_pass_id.index, 0);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::RENDER_PASS, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->render_pass_id = render_pass_id;
  this->is_replica = is_replica;
  this->mask_resource_id = mask_resource_id;
  this->contents_changed_since_last_frame = contents_changed_since_last_frame;
  this->mask_uv_rect = mask_uv_rect;
  this->filters = filters;
  this->background_filters = background_filters;
}

void RenderPassDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  if (mask_resource_id)
    mask_resource_id = callback.Run(mask_resource_id);
}

const RenderPassDrawQuad* RenderPassDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::RENDER_PASS);
  return static_cast<const RenderPassDrawQuad*>(quad);
}

void RenderPassDrawQuad::ExtendValue(base::DictionaryValue* value) const {
  value->Set("render_pass_id",
             TracedValue::CreateIDRef(render_pass_id.AsTracingId()).release());
  value->SetBoolean("is_replica", is_replica);
  value->SetInteger("mask_resource_id", mask_resource_id);
  value->Set("contents_changed_since_last_frame",
             MathUtil::AsValue(contents_changed_since_last_frame).release());
  value->Set("mask_uv_rect", MathUtil::AsValue(mask_uv_rect).release());
  value->Set("filters", filters.AsValue().release());
  value->Set("background_filters", background_filters.AsValue().release());
}

}  // namespace cc
