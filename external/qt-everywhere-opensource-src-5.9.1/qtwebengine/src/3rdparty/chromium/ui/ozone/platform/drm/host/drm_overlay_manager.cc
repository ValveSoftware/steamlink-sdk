// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

#include <stddef.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

typedef OverlayCandidatesOzone::OverlaySurfaceCandidateList
    OverlaySurfaceCandidateList;
typedef OverlayCandidatesOzone::OverlaySurfaceCandidate OverlaySurfaceCandidate;

namespace {
const size_t kMaxCacheSize = 200;
}  // namespace

DrmOverlayManager::DrmOverlayManager(GpuThreadAdapter* proxy,
                                     DrmWindowHostManager* window_manager)
    : proxy_(proxy), window_manager_(window_manager), cache_(kMaxCacheSize) {
  proxy_->RegisterHandlerForDrmOverlayManager(this);
}

DrmOverlayManager::~DrmOverlayManager() {
  proxy_->UnRegisterHandlerForDrmOverlayManager();
}

std::unique_ptr<OverlayCandidatesOzone>
DrmOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget w) {
  return base::MakeUnique<DrmOverlayCandidatesHost>(this, w);
}

void DrmOverlayManager::CheckOverlaySupport(
    OverlayCandidatesOzone::OverlaySurfaceCandidateList* candidates,
    gfx::AcceleratedWidget widget) {
  std::vector<OverlayCheck_Params> overlay_params;
  for (auto& candidate : *candidates) {
    // Reject candidates that don't fall on a pixel boundary.
    if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f)) {
      DCHECK(candidate.plane_z_order != 0);
      overlay_params.push_back(OverlayCheck_Params());
      overlay_params.back().is_overlay_candidate = false;
      continue;
    }

    // Compositor doesn't have information about the total size of primary
    // candidate. We get this information from display rect.
    if (candidate.plane_z_order == 0)
      candidate.buffer_size = gfx::ToNearestRect(candidate.display_rect).size();

    overlay_params.push_back(OverlayCheck_Params(candidate));
  }

  const auto& iter = cache_.Get(overlay_params);
  // We are still waiting on results for this candidate list from GPU.
  if (iter != cache_.end() && iter->second)
    return;

  size_t size = candidates->size();

  if (iter == cache_.end()) {
    // We can skip GPU side validation in case all candidates are invalid.
    bool needs_gpu_validation = false;
    for (size_t i = 0; i < size; i++) {
      if (!overlay_params.at(i).is_overlay_candidate)
        continue;

      const OverlaySurfaceCandidate& candidate = candidates->at(i);
      if (!CanHandleCandidate(candidate, widget)) {
        DCHECK(candidate.plane_z_order != 0);
        overlay_params.at(i).is_overlay_candidate = false;
        continue;
      }

      needs_gpu_validation = true;
    }

    cache_.Put(overlay_params, needs_gpu_validation);

    if (needs_gpu_validation)
      SendOverlayValidationRequest(overlay_params, widget);
  } else {
    const std::vector<OverlayCheck_Params>& validated_params = iter->first;
    DCHECK(size == validated_params.size());

    for (size_t i = 0; i < size; i++) {
      candidates->at(i).overlay_handled =
          validated_params.at(i).is_overlay_candidate;
    }
  }
}

void DrmOverlayManager::ResetCache() {
  cache_.Clear();
}

void DrmOverlayManager::SendOverlayValidationRequest(
    const std::vector<OverlayCheck_Params>& new_params,
    gfx::AcceleratedWidget widget) const {
  if (!proxy_->IsConnected())
    return;

  proxy_->GpuCheckOverlayCapabilities(widget, new_params);
}

void DrmOverlayManager::GpuSentOverlayResult(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& params) {
  cache_.Put(params, false);
}

bool DrmOverlayManager::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate,
    gfx::AcceleratedWidget widget) const {
  if (candidate.buffer_size.IsEmpty())
    return false;

  if (candidate.transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  if (candidate.plane_z_order != 0) {
    // It is possible that the cc rect we get actually falls off the edge of
    // the screen. Usually this is prevented via things like status bars
    // blocking overlaying or cc clipping it, but in case it wasn't properly
    // clipped (since GL will render this situation fine) just ignore it
    // here. This should be an extremely rare occurrance.
    DrmWindowHost* window = window_manager_->GetWindow(widget);
    if (!window->GetBounds().Contains(
            gfx::ToNearestRect(candidate.display_rect))) {
      return false;
    }
  }

  if (candidate.is_clipped &&
      !candidate.clip_rect.Contains(candidate.quad_rect_in_target_space))
    return false;

  return true;
}

}  // namespace ui
