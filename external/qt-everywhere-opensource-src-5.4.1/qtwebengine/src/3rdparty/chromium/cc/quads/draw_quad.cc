// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/draw_quad.h"

#include "base/logging.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "ui/gfx/quad_f.h"

namespace {
template<typename T> T* TypedCopy(const cc::DrawQuad* other) {
  return new T(*T::MaterialCast(other));
}
}  // namespace

namespace cc {

DrawQuad::DrawQuad()
    : material(INVALID),
      needs_blending(false),
      shared_quad_state() {
}

void DrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                      Material material,
                      const gfx::Rect& rect,
                      const gfx::Rect& opaque_rect,
                      const gfx::Rect& visible_rect,
                      bool needs_blending) {
  DCHECK(rect.Contains(visible_rect)) << "rect: " << rect.ToString()
                                      << " visible_rect: "
                                      << visible_rect.ToString();
  DCHECK(opaque_rect.IsEmpty() || rect.Contains(opaque_rect))
      << "rect: " << rect.ToString() << "opaque_rect "
      << opaque_rect.ToString();

  this->material = material;
  this->rect = rect;
  this->opaque_rect = opaque_rect;
  this->visible_rect = visible_rect;
  this->needs_blending = needs_blending;
  this->shared_quad_state = shared_quad_state;

  DCHECK(shared_quad_state);
  DCHECK(material != INVALID);
}

DrawQuad::~DrawQuad() {
}

scoped_ptr<DrawQuad> DrawQuad::Copy(
    const SharedQuadState* copied_shared_quad_state) const {
  scoped_ptr<DrawQuad> copy_quad;
  switch (material) {
    case CHECKERBOARD:
      copy_quad.reset(TypedCopy<CheckerboardDrawQuad>(this));
      break;
    case DEBUG_BORDER:
      copy_quad.reset(TypedCopy<DebugBorderDrawQuad>(this));
      break;
    case IO_SURFACE_CONTENT:
      copy_quad.reset(TypedCopy<IOSurfaceDrawQuad>(this));
      break;
    case PICTURE_CONTENT:
      copy_quad.reset(TypedCopy<PictureDrawQuad>(this));
      break;
    case TEXTURE_CONTENT:
      copy_quad.reset(TypedCopy<TextureDrawQuad>(this));
      break;
    case SOLID_COLOR:
      copy_quad.reset(TypedCopy<SolidColorDrawQuad>(this));
      break;
    case TILED_CONTENT:
      copy_quad.reset(TypedCopy<TileDrawQuad>(this));
      break;
    case STREAM_VIDEO_CONTENT:
      copy_quad.reset(TypedCopy<StreamVideoDrawQuad>(this));
      break;
    case SURFACE_CONTENT:
      copy_quad.reset(TypedCopy<SurfaceDrawQuad>(this));
      break;
    case YUV_VIDEO_CONTENT:
      copy_quad.reset(TypedCopy<YUVVideoDrawQuad>(this));
      break;
    case RENDER_PASS:  // RenderPass quads have their own copy() method.
    case INVALID:
      LOG(FATAL) << "Invalid DrawQuad material " << material;
      break;
  }
  copy_quad->shared_quad_state = copied_shared_quad_state;
  return copy_quad.Pass();
}

scoped_ptr<base::Value> DrawQuad::AsValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetInteger("material", material);
  value->Set("shared_state",
             TracedValue::CreateIDRef(shared_quad_state).release());

  value->Set("content_space_rect", MathUtil::AsValue(rect).release());
  bool rect_is_clipped;
  gfx::QuadF rect_as_target_space_quad = MathUtil::MapQuad(
      shared_quad_state->content_to_target_transform,
      gfx::QuadF(rect),
      &rect_is_clipped);
  value->Set("rect_as_target_space_quad",
             MathUtil::AsValue(rect_as_target_space_quad).release());
  value->SetBoolean("rect_is_clipped", rect_is_clipped);

  value->Set("content_space_opaque_rect",
             MathUtil::AsValue(opaque_rect).release());
  bool opaque_rect_is_clipped;
  gfx::QuadF opaque_rect_as_target_space_quad = MathUtil::MapQuad(
      shared_quad_state->content_to_target_transform,
      gfx::QuadF(opaque_rect),
      &opaque_rect_is_clipped);
  value->Set("opaque_rect_as_target_space_quad",
             MathUtil::AsValue(opaque_rect_as_target_space_quad).release());
  value->SetBoolean("opaque_rect_is_clipped", opaque_rect_is_clipped);

  value->Set("content_space_visible_rect",
             MathUtil::AsValue(visible_rect).release());
  bool visible_rect_is_clipped;
  gfx::QuadF visible_rect_as_target_space_quad = MathUtil::MapQuad(
      shared_quad_state->content_to_target_transform,
      gfx::QuadF(visible_rect),
      &visible_rect_is_clipped);
  value->Set("visible_rect_as_target_space_quad",
             MathUtil::AsValue(visible_rect_as_target_space_quad).release());
  value->SetBoolean("visible_rect_is_clipped", visible_rect_is_clipped);

  value->SetBoolean("needs_blending", needs_blending);
  value->SetBoolean("should_draw_with_blending", ShouldDrawWithBlending());
  ExtendValue(value.get());
  return value.PassAs<base::Value>();
}

}  // namespace cc
