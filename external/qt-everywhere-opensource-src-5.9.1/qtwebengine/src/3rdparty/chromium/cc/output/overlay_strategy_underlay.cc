// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_underlay.h"

#include "cc/output/overlay_candidate_validator.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"

namespace cc {

OverlayStrategyUnderlay::OverlayStrategyUnderlay(
    OverlayCandidateValidator* capability_checker)
    : capability_checker_(capability_checker) {
  DCHECK(capability_checker);
}

OverlayStrategyUnderlay::~OverlayStrategyUnderlay() {}

bool OverlayStrategyUnderlay::Attempt(ResourceProvider* resource_provider,
                                      RenderPass* render_pass,
                                      OverlayCandidateList* candidate_list) {
  QuadList& quad_list = render_pass->quad_list;
  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    OverlayCandidate candidate;
    if (!OverlayCandidate::FromDrawQuad(resource_provider, *it, &candidate))
      continue;

    // Add the overlay.
    OverlayCandidateList new_candidate_list = *candidate_list;
    new_candidate_list.push_back(candidate);
    new_candidate_list.back().plane_z_order = -1;

    // Check for support.
    capability_checker_->CheckOverlaySupport(&new_candidate_list);

    // If the candidate can be handled by an overlay, create a pass for it. We
    // need to switch out the video quad with a black transparent one.
    if (new_candidate_list.back().overlay_handled) {
      new_candidate_list.back().is_unoccluded =
          !OverlayCandidate::IsOccluded(candidate, quad_list.cbegin(), it);
      const SharedQuadState* shared_quad_state = it->shared_quad_state;
      gfx::Rect rect = it->visible_rect;
      SolidColorDrawQuad* replacement =
          quad_list.ReplaceExistingElement<SolidColorDrawQuad>(it);
      replacement->SetAll(shared_quad_state, rect, rect, rect, false,
                          SK_ColorTRANSPARENT, true);
      candidate_list->swap(new_candidate_list);
      return true;
    }
  }

  return false;
}

}  // namespace cc
