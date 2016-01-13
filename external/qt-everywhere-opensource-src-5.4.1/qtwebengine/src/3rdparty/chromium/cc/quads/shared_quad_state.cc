// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/shared_quad_state.h"

#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"

namespace cc {

SharedQuadState::SharedQuadState()
    : is_clipped(false),
      opacity(0.f),
      blend_mode(SkXfermode::kSrcOver_Mode),
      sorting_context_id(0) {
}

SharedQuadState::~SharedQuadState() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      "cc::SharedQuadState", this);
}

void SharedQuadState::CopyFrom(const SharedQuadState* other) {
  *this = *other;
}

void SharedQuadState::SetAll(const gfx::Transform& content_to_target_transform,
                             const gfx::Size& content_bounds,
                             const gfx::Rect& visible_content_rect,
                             const gfx::Rect& clip_rect,
                             bool is_clipped,
                             float opacity,
                             SkXfermode::Mode blend_mode,
                             int sorting_context_id) {
  this->content_to_target_transform = content_to_target_transform;
  this->content_bounds = content_bounds;
  this->visible_content_rect = visible_content_rect;
  this->clip_rect = clip_rect;
  this->is_clipped = is_clipped;
  this->opacity = opacity;
  this->blend_mode = blend_mode;
  this->sorting_context_id = sorting_context_id;
}

scoped_ptr<base::Value> SharedQuadState::AsValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("transform",
             MathUtil::AsValue(content_to_target_transform).release());
  value->Set("layer_content_bounds",
             MathUtil::AsValue(content_bounds).release());
  value->Set("layer_visible_content_rect",
             MathUtil::AsValue(visible_content_rect).release());
  value->SetBoolean("is_clipped", is_clipped);
  value->Set("clip_rect", MathUtil::AsValue(clip_rect).release());
  value->SetDouble("opacity", opacity);
  value->SetString("blend_mode", SkXfermode::ModeName(blend_mode));
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      value.get(), "cc::SharedQuadState", this);
  return value.PassAs<base::Value>();
}

}  // namespace cc
