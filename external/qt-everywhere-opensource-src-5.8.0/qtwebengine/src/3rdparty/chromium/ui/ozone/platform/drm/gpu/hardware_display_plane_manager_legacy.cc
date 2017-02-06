// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

#include <errno.h>

#include "base/bind.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

HardwareDisplayPlaneManagerLegacy::HardwareDisplayPlaneManagerLegacy() {
}

HardwareDisplayPlaneManagerLegacy::~HardwareDisplayPlaneManagerLegacy() {
}

bool HardwareDisplayPlaneManagerLegacy::Commit(
    HardwareDisplayPlaneList* plane_list,
    bool test_only) {
  if (test_only) {
    for (HardwareDisplayPlane* plane : plane_list->plane_list) {
      plane->set_in_use(false);
    }
    plane_list->plane_list.clear();
    plane_list->legacy_page_flips.clear();
    return true;
  }
  if (plane_list->plane_list.empty())  // No assigned planes, nothing to do.
    return true;
  bool ret = true;
  // The order of operations here (set new planes, pageflip, clear old planes)
  // is designed to minimze the chance of a significant artifact occurring.
  // The planes must be updated first because the main plane no longer contains
  // their content. The old planes are removed last because the previous primary
  // plane used them as overlays and thus didn't contain their content, so we
  // must first flip to the new primary plane, which does. The error here will
  // be the delta of (new contents, old contents), but it should be barely
  // noticeable.
  for (const auto& flip : plane_list->legacy_page_flips) {
    for (const auto& plane : flip.planes) {
      if (!drm_->PageFlipOverlay(flip.crtc_id, plane.framebuffer, plane.bounds,
                                 plane.src_rect, plane.plane)) {
        PLOG(ERROR) << "Cannot display plane on overlay: crtc=" << flip.crtc
                    << " plane=" << plane.plane;
        ret = false;
        flip.crtc->SignalPageFlipRequest(gfx::SwapResult::SWAP_FAILED);
        break;
      }
    }
    if (!drm_->PageFlip(flip.crtc_id, flip.framebuffer,
                        base::Bind(&CrtcController::OnPageFlipEvent,
                                   flip.crtc->AsWeakPtr()))) {
      // 1) Permission Denied is a legitimate error.
      // 2) Device or resource busy is possible if we're page flipping a
      // disconnected CRTC. Pretend we're fine since a hotplug event is supposed
      // to be on its way.
      // NOTE: We could be getting EBUSY if we're trying to page flip a CRTC
      // that has a pending page flip, however the contract is that the caller
      // will never attempt this (since the caller should be waiting for the
      // page flip completion message).
      if (errno != EACCES && errno != EBUSY) {
        PLOG(ERROR) << "Cannot page flip: crtc=" << flip.crtc_id
                    << " framebuffer=" << flip.framebuffer;
        ret = false;
      }
      flip.crtc->SignalPageFlipRequest(ret ? gfx::SwapResult::SWAP_ACK
                                           : gfx::SwapResult::SWAP_FAILED);
    }
  }
  // For each element in |old_plane_list|, if it hasn't been reclaimed (by
  // this or any other HDPL), clear the overlay contents.
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (!plane->in_use() && (plane->type() != HardwareDisplayPlane::kDummy)) {
      // This plane is being released, so we need to zero it.
      if (!drm_->PageFlipOverlay(plane->owning_crtc(), 0, gfx::Rect(),
                                 gfx::Rect(), plane->plane_id())) {
        PLOG(ERROR) << "Cannot free overlay: crtc=" << plane->owning_crtc()
                    << " plane=" << plane->plane_id();
        ret = false;
        break;
      }
    }
  }

  if (ret) {
    plane_list->plane_list.swap(plane_list->old_plane_list);
    plane_list->plane_list.clear();
    plane_list->legacy_page_flips.clear();
  } else {
    ResetCurrentPlaneList(plane_list);
  }

  return ret;
}

bool HardwareDisplayPlaneManagerLegacy::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const OverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  if ((hw_plane->type() == HardwareDisplayPlane::kDummy) ||
      plane_list->legacy_page_flips.empty() ||
      plane_list->legacy_page_flips.back().crtc_id != crtc_id) {
    plane_list->legacy_page_flips.push_back(
        HardwareDisplayPlaneList::PageFlipInfo(
            crtc_id, overlay.buffer->GetFramebufferId(), crtc));
  } else {
    plane_list->legacy_page_flips.back().planes.push_back(
        HardwareDisplayPlaneList::PageFlipInfo::Plane(
            hw_plane->plane_id(), overlay.buffer->GetFramebufferId(),
            overlay.display_bounds, src_rect));
  }
  return true;
}

}  // namespace ui
