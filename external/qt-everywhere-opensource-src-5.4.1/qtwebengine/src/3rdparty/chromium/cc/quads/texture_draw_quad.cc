// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/texture_draw_quad.h"

#include "base/logging.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

TextureDrawQuad::TextureDrawQuad()
    : resource_id(0),
      premultiplied_alpha(false),
      background_color(SK_ColorTRANSPARENT),
      flipped(false) {
  this->vertex_opacity[0] = 0.f;
  this->vertex_opacity[1] = 0.f;
  this->vertex_opacity[2] = 0.f;
  this->vertex_opacity[3] = 0.f;
}

scoped_ptr<TextureDrawQuad> TextureDrawQuad::Create() {
  return make_scoped_ptr(new TextureDrawQuad);
}

void TextureDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& opaque_rect,
                             const gfx::Rect& visible_rect,
                             unsigned resource_id,
                             bool premultiplied_alpha,
                             const gfx::PointF& uv_top_left,
                             const gfx::PointF& uv_bottom_right,
                             SkColor background_color,
                             const float vertex_opacity[4],
                             bool flipped) {
  bool needs_blending = vertex_opacity[0] != 1.0f || vertex_opacity[1] != 1.0f
      || vertex_opacity[2] != 1.0f || vertex_opacity[3] != 1.0f;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_top_left = uv_top_left;
  this->uv_bottom_right = uv_bottom_right;
  this->background_color = background_color;
  this->vertex_opacity[0] = vertex_opacity[0];
  this->vertex_opacity[1] = vertex_opacity[1];
  this->vertex_opacity[2] = vertex_opacity[2];
  this->vertex_opacity[3] = vertex_opacity[3];
  this->flipped = flipped;
}

void TextureDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& opaque_rect,
                             const gfx::Rect& visible_rect, bool needs_blending,
                             unsigned resource_id, bool premultiplied_alpha,
                             const gfx::PointF& uv_top_left,
                             const gfx::PointF& uv_bottom_right,
                             SkColor background_color,
                             const float vertex_opacity[4],
                             bool flipped) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_top_left = uv_top_left;
  this->uv_bottom_right = uv_bottom_right;
  this->background_color = background_color;
  this->vertex_opacity[0] = vertex_opacity[0];
  this->vertex_opacity[1] = vertex_opacity[1];
  this->vertex_opacity[2] = vertex_opacity[2];
  this->vertex_opacity[3] = vertex_opacity[3];
  this->flipped = flipped;
}

void TextureDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  resource_id = callback.Run(resource_id);
}

const TextureDrawQuad* TextureDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::TEXTURE_CONTENT);
  return static_cast<const TextureDrawQuad*>(quad);
}

void TextureDrawQuad::ExtendValue(base::DictionaryValue* value) const {
  value->SetInteger("resource_id", resource_id);
  value->SetBoolean("premultiplied_alpha", premultiplied_alpha);
  value->Set("uv_top_left", MathUtil::AsValue(uv_top_left).release());
  value->Set("uv_bottom_right", MathUtil::AsValue(uv_bottom_right).release());
  value->SetInteger("background_color", background_color);
  scoped_ptr<base::ListValue> vertex_opacity_value(new base::ListValue);
  for (size_t i = 0; i < 4; ++i)
    vertex_opacity_value->AppendDouble(vertex_opacity[i]);
  value->Set("vertex_opacity", vertex_opacity_value.release());
  value->SetBoolean("flipped", flipped);
}

}  // namespace cc
