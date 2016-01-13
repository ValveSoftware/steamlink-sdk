// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/picture_draw_quad.h"

#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/resources/platform_color.h"

namespace cc {

PictureDrawQuad::PictureDrawQuad() {
}

PictureDrawQuad::~PictureDrawQuad() {
}

scoped_ptr<PictureDrawQuad> PictureDrawQuad::Create() {
  return make_scoped_ptr(new PictureDrawQuad);
}

void PictureDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& opaque_rect,
                             const gfx::Rect& visible_rect,
                             const gfx::RectF& tex_coord_rect,
                             const gfx::Size& texture_size,
                             ResourceFormat texture_format,
                             const gfx::Rect& content_rect,
                             float contents_scale,
                             scoped_refptr<PicturePileImpl> picture_pile) {
  ContentDrawQuadBase::SetNew(
      shared_quad_state,
      DrawQuad::PICTURE_CONTENT,
      rect,
      opaque_rect,
      visible_rect,
      tex_coord_rect,
      texture_size,
      !PlatformColor::SameComponentOrder(texture_format));
  this->content_rect = content_rect;
  this->contents_scale = contents_scale;
  this->picture_pile = picture_pile;
  this->texture_format = texture_format;
}

void PictureDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& opaque_rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const gfx::RectF& tex_coord_rect,
                             const gfx::Size& texture_size,
                             ResourceFormat texture_format,
                             const gfx::Rect& content_rect,
                             float contents_scale,
                             scoped_refptr<PicturePileImpl> picture_pile) {
  ContentDrawQuadBase::SetAll(shared_quad_state,
                              DrawQuad::PICTURE_CONTENT,
                              rect,
                              opaque_rect,
                              visible_rect,
                              needs_blending,
                              tex_coord_rect,
                              texture_size,
                              !PlatformColor::SameComponentOrder(
                                  texture_format));
  this->content_rect = content_rect;
  this->contents_scale = contents_scale;
  this->picture_pile = picture_pile;
  this->texture_format = texture_format;
}

void PictureDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  // TODO(danakj): Convert to TextureDrawQuad?
  NOTIMPLEMENTED();
}

const PictureDrawQuad* PictureDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::PICTURE_CONTENT);
  return static_cast<const PictureDrawQuad*>(quad);
}

void PictureDrawQuad::ExtendValue(base::DictionaryValue* value) const {
  ContentDrawQuadBase::ExtendValue(value);
  value->Set("content_rect", MathUtil::AsValue(content_rect).release());
  value->SetDouble("contents_scale", contents_scale);
  value->SetInteger("texture_format", texture_format);
  // TODO(piman): picture_pile?
}

}  // namespace cc
