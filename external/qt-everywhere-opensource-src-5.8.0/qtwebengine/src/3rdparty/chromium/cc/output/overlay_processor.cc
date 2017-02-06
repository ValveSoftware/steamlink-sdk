// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_processor.h"

#include "cc/output/output_surface.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "cc/quads/draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

OverlayProcessor::OverlayProcessor(OutputSurface* surface) : surface_(surface) {
}

void OverlayProcessor::Initialize() {
  DCHECK(surface_);
  OverlayCandidateValidator* validator =
      surface_->GetOverlayCandidateValidator();
  if (validator)
    validator->GetStrategies(&strategies_);
}

OverlayProcessor::~OverlayProcessor() {}

gfx::Rect OverlayProcessor::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

bool OverlayProcessor::ProcessForCALayers(
    ResourceProvider* resource_provider,
    RenderPass* render_pass,
    OverlayCandidateList* overlay_candidates,
    CALayerOverlayList* ca_layer_overlays,
    gfx::Rect* damage_rect) {
  OverlayCandidateValidator* overlay_validator =
      surface_->GetOverlayCandidateValidator();
  if (!overlay_validator || !overlay_validator->AllowCALayerOverlays())
    return false;

  if (!ProcessForCALayerOverlays(resource_provider,
                                 gfx::RectF(render_pass->output_rect),
                                 render_pass->quad_list, ca_layer_overlays))
    return false;

  // CALayer overlays are all-or-nothing. If all quads were replaced with
  // layers then clear the list and remove the backbuffer from the overcandidate
  // list.
  overlay_candidates->clear();
  render_pass->quad_list.clear();
  overlay_damage_rect_ = render_pass->output_rect;
  *damage_rect = gfx::Rect();
  return true;
}

void OverlayProcessor::ProcessForOverlays(ResourceProvider* resource_provider,
                                          RenderPass* render_pass,
                                          OverlayCandidateList* candidates,
                                          CALayerOverlayList* ca_layer_overlays,
                                          gfx::Rect* damage_rect) {
  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (!render_pass->copy_requests.empty()) {
    // If overlay processing was skipped for a frame there's no way to be sure
    // of the state of the previous frame, so reset.
    previous_frame_underlay_rect_ = gfx::Rect();
    return;
  }

  // First attempt to process for CALayers.
  if (ProcessForCALayers(resource_provider, render_pass, candidates,
                         ca_layer_overlays, damage_rect)) {
    return;
  }

  // Only if that fails, attempt hardware overlay strategies.
  for (const auto& strategy : strategies_) {
    if (!strategy->Attempt(resource_provider, render_pass, candidates))
      continue;

    UpdateDamageRect(candidates, damage_rect);
    return;
  }
}

// Subtract on-top overlays from the damage rect, unless the overlays use
// the backbuffer as their content (in which case, add their combined rect
// back to the damage at the end).
// Also subtract unoccluded underlays from the damage rect if we know that the
// same underlay was scheduled on the previous frame. If the renderer decides
// not to swap the framebuffer there will still be a transparent hole in the
// previous frame. This only handles the common case of a single underlay quad
// for fullscreen video.
void OverlayProcessor::UpdateDamageRect(OverlayCandidateList* candidates,
                                        gfx::Rect* damage_rect) {
  gfx::Rect output_surface_overlay_damage_rect;
  gfx::Rect this_frame_underlay_rect;
  for (const OverlayCandidate& overlay : *candidates) {
    if (overlay.plane_z_order > 0) {
      const gfx::Rect overlay_display_rect =
          ToEnclosedRect(overlay.display_rect);
      overlay_damage_rect_.Union(overlay_display_rect);
      damage_rect->Subtract(overlay_display_rect);
      if (overlay.use_output_surface_for_resource)
        output_surface_overlay_damage_rect.Union(overlay_display_rect);
    } else if (overlay.plane_z_order < 0 && overlay.is_unoccluded &&
               this_frame_underlay_rect.IsEmpty()) {
      this_frame_underlay_rect = ToEnclosedRect(overlay.display_rect);
    }
  }

  if (this_frame_underlay_rect == previous_frame_underlay_rect_)
    damage_rect->Subtract(this_frame_underlay_rect);
  previous_frame_underlay_rect_ = this_frame_underlay_rect;

  damage_rect->Union(output_surface_overlay_damage_rect);
}

}  // namespace cc
