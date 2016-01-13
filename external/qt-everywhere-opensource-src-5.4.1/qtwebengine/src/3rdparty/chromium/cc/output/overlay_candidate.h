// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_CANDIDATE_H_
#define CC_OUTPUT_OVERLAY_CANDIDATE_H_

#include <vector>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource_format.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/transform.h"

namespace cc {

class CC_EXPORT OverlayCandidate {
 public:
  static gfx::OverlayTransform GetOverlayTransform(
      const gfx::Transform& quad_transform,
      bool flipped);
  static gfx::Rect GetOverlayRect(const gfx::Transform& quad_transform,
                                  const gfx::Rect& rect);

  OverlayCandidate();
  ~OverlayCandidate();

  // Transformation to apply to layer during composition.
  gfx::OverlayTransform transform;
  // Format of the buffer to composite.
  ResourceFormat format;
  // Rect on the display to position the overlay to.
  gfx::Rect display_rect;
  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF uv_rect;
  // Texture resource to present in an overlay.
  unsigned resource_id;
  // Stacking order of the overlay plane relative to the main surface,
  // which is 0. Signed to allow for "underlays".
  int plane_z_order;

  // To be modified by the implementer if this candidate can go into
  // an overlay.
  bool overlay_handled;
};

typedef std::vector<OverlayCandidate> OverlayCandidateList;

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_CANDIDATE_H_
