// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/content_draw_quad_base.h"

#include "base/logging.h"
#include "base/values.h"
#include "cc/base/math_util.h"

namespace cc {

ContentDrawQuadBase::ContentDrawQuadBase()
    : swizzle_contents(false) {
}

ContentDrawQuadBase::~ContentDrawQuadBase() {
}

void ContentDrawQuadBase::SetNew(const SharedQuadState* shared_quad_state,
                                 DrawQuad::Material material,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& opaque_rect,
                                 const gfx::Rect& visible_rect,
                                 const gfx::RectF& tex_coord_rect,
                                 const gfx::Size& texture_size,
                                 bool swizzle_contents) {
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, material, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
}

void ContentDrawQuadBase::SetAll(const SharedQuadState* shared_quad_state,
                                 DrawQuad::Material material,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& opaque_rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 const gfx::RectF& tex_coord_rect,
                                 const gfx::Size& texture_size,
                                 bool swizzle_contents) {
  DrawQuad::SetAll(shared_quad_state, material, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
}

void ContentDrawQuadBase::ExtendValue(base::DictionaryValue* value) const {
  value->Set("tex_coord_rect", MathUtil::AsValue(tex_coord_rect).release());
  value->Set("texture_size", MathUtil::AsValue(texture_size).release());
  value->SetBoolean("swizzle_contents", swizzle_contents);
}

}  // namespace cc
