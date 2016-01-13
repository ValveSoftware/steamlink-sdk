// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/tile_draw_quad.h"

#include "base/logging.h"
#include "base/values.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

TileDrawQuad::TileDrawQuad()
    : resource_id(0) {
}

TileDrawQuad::~TileDrawQuad() {
}

scoped_ptr<TileDrawQuad> TileDrawQuad::Create() {
  return make_scoped_ptr(new TileDrawQuad);
}

void TileDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                          const gfx::Rect& rect,
                          const gfx::Rect& opaque_rect,
                          const gfx::Rect& visible_rect,
                          unsigned resource_id,
                          const gfx::RectF& tex_coord_rect,
                          const gfx::Size& texture_size,
                          bool swizzle_contents) {
  ContentDrawQuadBase::SetNew(shared_quad_state,
                              DrawQuad::TILED_CONTENT,
                              rect,
                              opaque_rect,
                              visible_rect,
                              tex_coord_rect,
                              texture_size,
                              swizzle_contents);
  this->resource_id = resource_id;
}

void TileDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                          const gfx::Rect& rect,
                          const gfx::Rect& opaque_rect,
                          const gfx::Rect& visible_rect,
                          bool needs_blending,
                          unsigned resource_id,
                          const gfx::RectF& tex_coord_rect,
                          const gfx::Size& texture_size,
                          bool swizzle_contents) {
  ContentDrawQuadBase::SetAll(shared_quad_state, DrawQuad::TILED_CONTENT, rect,
                              opaque_rect, visible_rect, needs_blending,
                              tex_coord_rect, texture_size, swizzle_contents);
  this->resource_id = resource_id;
}

void TileDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  resource_id = callback.Run(resource_id);
}

const TileDrawQuad* TileDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::TILED_CONTENT);
  return static_cast<const TileDrawQuad*>(quad);
}

void TileDrawQuad::ExtendValue(base::DictionaryValue* value) const {
  ContentDrawQuadBase::ExtendValue(value);
  value->SetInteger("resource_id", resource_id);
}

}  // namespace cc
