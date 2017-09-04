// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_single_on_top.h"

#include "cc/base/math_util.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/quads/draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

OverlayStrategySingleOnTop::OverlayStrategySingleOnTop(
    OverlayCandidateValidator* capability_checker)
    : capability_checker_(capability_checker) {
  DCHECK(capability_checker);
}

OverlayStrategySingleOnTop::~OverlayStrategySingleOnTop() {}

bool OverlayStrategySingleOnTop::Attempt(ResourceProvider* resource_provider,
                                         RenderPass* render_pass,
                                         OverlayCandidateList* candidate_list) {
  QuadList* quad_list = &render_pass->quad_list;
  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    OverlayCandidate candidate;
    if (OverlayCandidate::FromDrawQuad(resource_provider, *it, &candidate) &&
        TryOverlay(quad_list, candidate_list, candidate, it)) {
      return true;
    }
  }

  return false;
}

bool OverlayStrategySingleOnTop::TryOverlay(
    QuadList* quad_list,
    OverlayCandidateList* candidate_list,
    const OverlayCandidate& candidate,
    QuadList::Iterator candidate_iterator) {
  // Check that no prior quads overlap it.
  if (OverlayCandidate::IsOccluded(candidate, quad_list->cbegin(),
                                   candidate_iterator))
    return false;

  // Add the overlay.
  OverlayCandidateList new_candidate_list = *candidate_list;
  new_candidate_list.push_back(candidate);
  new_candidate_list.back().plane_z_order = 1;

  // Check for support.
  capability_checker_->CheckOverlaySupport(&new_candidate_list);

  const OverlayCandidate& overlay_candidate = new_candidate_list.back();
  // If the candidate can be handled by an overlay, create a pass for it.
  if (overlay_candidate.overlay_handled) {
    quad_list->EraseAndInvalidateAllPointers(candidate_iterator);
    candidate_list->swap(new_candidate_list);
    return true;
  }

  return false;
}

}  // namespace cc
