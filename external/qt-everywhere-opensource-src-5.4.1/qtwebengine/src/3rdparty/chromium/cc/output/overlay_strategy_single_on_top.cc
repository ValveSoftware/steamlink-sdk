// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_single_on_top.h"

#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

OverlayStrategySingleOnTop::OverlayStrategySingleOnTop(
    OverlayCandidateValidator* capability_checker,
    ResourceProvider* resource_provider)
    : capability_checker_(capability_checker),
      resource_provider_(resource_provider) {}

bool OverlayStrategySingleOnTop::Attempt(
    RenderPassList* render_passes_in_draw_order,
    OverlayCandidateList* candidate_list) {
  // Only attempt to handle very simple case for now.
  if (!capability_checker_)
    return false;

  RenderPass* root_render_pass = render_passes_in_draw_order->back();
  DCHECK(root_render_pass);

  QuadList& quad_list = root_render_pass->quad_list;
  const DrawQuad* candidate_quad = quad_list.front();
  if (candidate_quad->material != DrawQuad::TEXTURE_CONTENT)
    return false;

  const TextureDrawQuad& quad = *TextureDrawQuad::MaterialCast(candidate_quad);
  if (!resource_provider_->AllowOverlay(quad.resource_id))
    return false;

  // Simple quads only.
  gfx::OverlayTransform overlay_transform =
      OverlayCandidate::GetOverlayTransform(quad.quadTransform(), quad.flipped);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID ||
      !quad.quadTransform().IsIdentityOrTranslation() || quad.needs_blending ||
      quad.shared_quad_state->opacity != 1.f ||
      quad.shared_quad_state->blend_mode != SkXfermode::kSrcOver_Mode ||
      quad.premultiplied_alpha || quad.background_color != SK_ColorTRANSPARENT)
    return false;

  // Add our primary surface.
  OverlayCandidateList candidates;
  OverlayCandidate main_image;
  main_image.display_rect = root_render_pass->output_rect;
  main_image.format = RGBA_8888;
  candidates.push_back(main_image);

  // Add the overlay.
  OverlayCandidate candidate;
  candidate.transform = overlay_transform;
  candidate.display_rect =
      OverlayCandidate::GetOverlayRect(quad.quadTransform(), quad.rect);
  candidate.uv_rect = BoundingRect(quad.uv_top_left, quad.uv_bottom_right);
  candidate.format = RGBA_8888;
  candidate.resource_id = quad.resource_id;
  candidate.plane_z_order = 1;
  candidates.push_back(candidate);

  // Check for support.
  capability_checker_->CheckOverlaySupport(&candidates);

  // If the candidate can be handled by an overlay, create a pass for it.
  if (candidates[1].overlay_handled) {
    scoped_ptr<DrawQuad> overlay_quad = quad_list.take(quad_list.begin());
    quad_list.erase(quad_list.begin());
    candidate_list->swap(candidates);
    return true;
  }
  return false;
}

}  // namespace cc
