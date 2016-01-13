// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_candidate.h"

#include "ui/gfx/rect_conversions.h"

namespace cc {

OverlayCandidate::OverlayCandidate()
    : transform(gfx::OVERLAY_TRANSFORM_NONE),
      format(RGBA_8888),
      uv_rect(0.f, 0.f, 1.f, 1.f),
      resource_id(0),
      plane_z_order(0),
      overlay_handled(false) {}

OverlayCandidate::~OverlayCandidate() {}

// static
gfx::OverlayTransform OverlayCandidate::GetOverlayTransform(
    const gfx::Transform& quad_transform,
    bool flipped) {
  if (!quad_transform.IsIdentityOrTranslation())
    return gfx::OVERLAY_TRANSFORM_INVALID;

  return flipped ? gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL
                 : gfx::OVERLAY_TRANSFORM_NONE;
}

// static
gfx::Rect OverlayCandidate::GetOverlayRect(const gfx::Transform& quad_transform,
                                           const gfx::Rect& rect) {
  DCHECK(quad_transform.IsIdentityOrTranslation());

  gfx::RectF float_rect(rect);
  quad_transform.TransformRect(&float_rect);
  return gfx::ToNearestRect(float_rect);
}

}  // namespace cc
