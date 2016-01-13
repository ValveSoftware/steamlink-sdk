// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/checkerboard_draw_quad.h"

#include "base/logging.h"
#include "base/values.h"

namespace cc {

CheckerboardDrawQuad::CheckerboardDrawQuad() : color(0) {}

scoped_ptr<CheckerboardDrawQuad> CheckerboardDrawQuad::Create() {
  return make_scoped_ptr(new CheckerboardDrawQuad);
}

void CheckerboardDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                  const gfx::Rect& rect,
                                  const gfx::Rect& visible_rect,
                                  SkColor color) {
  gfx::Rect opaque_rect = SkColorGetA(color) == 255 ? rect : gfx::Rect();
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::CHECKERBOARD, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
}

void CheckerboardDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                  const gfx::Rect& rect,
                                  const gfx::Rect& opaque_rect,
                                  const gfx::Rect& visible_rect,
                                  bool needs_blending,
                                  SkColor color) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::CHECKERBOARD, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
}

void CheckerboardDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {}

const CheckerboardDrawQuad* CheckerboardDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::CHECKERBOARD);
  return static_cast<const CheckerboardDrawQuad*>(quad);
}

void CheckerboardDrawQuad::ExtendValue(base::DictionaryValue* value) const {
  value->SetInteger("color", color);
}

}  // namespace cc
