// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"

#include "base/bind.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

namespace {

void AtomicPageFlipCallback(std::vector<base::WeakPtr<CrtcController>> crtcs,
                            unsigned int frame,
                            unsigned int seconds,
                            unsigned int useconds) {
  for (auto& crtc : crtcs) {
    auto* crtc_ptr = crtc.get();
    if (crtc_ptr)
      crtc_ptr->OnPageFlipEvent(frame, seconds, useconds);
  }
}

}  // namespace

HardwareDisplayPlaneManagerAtomic::HardwareDisplayPlaneManagerAtomic() {
}

HardwareDisplayPlaneManagerAtomic::~HardwareDisplayPlaneManagerAtomic() {
}

bool HardwareDisplayPlaneManagerAtomic::Commit(
    HardwareDisplayPlaneList* plane_list,
    bool test_only) {
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    bool found =
        std::find(plane_list->plane_list.begin(), plane_list->plane_list.end(),
                  plane) != plane_list->plane_list.end();
    if (!found) {
      // This plane is being released, so we need to zero it.
      plane->set_in_use(false);
      HardwareDisplayPlaneAtomic* atomic_plane =
          static_cast<HardwareDisplayPlaneAtomic*>(plane);
      atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(), 0, 0,
                                 gfx::Rect(), gfx::Rect());
    }
  }

  std::vector<base::WeakPtr<CrtcController>> crtcs;
  for (HardwareDisplayPlane* plane : plane_list->plane_list) {
    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    if (crtcs.empty() || crtcs.back().get() != atomic_plane->crtc())
      crtcs.push_back(atomic_plane->crtc()->AsWeakPtr());
  }

  if (test_only) {
    for (HardwareDisplayPlane* plane : plane_list->plane_list) {
      plane->set_in_use(false);
    }
  } else {
    plane_list->plane_list.swap(plane_list->old_plane_list);
  }

  uint32_t flags = 0;
  if (test_only) {
    flags = DRM_MODE_ATOMIC_TEST_ONLY;
  } else {
    flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;
  }

  if (!drm_->CommitProperties(plane_list->atomic_property_set.get(), flags,
                              crtcs.size(),
                              base::Bind(&AtomicPageFlipCallback, crtcs))) {
    PLOG(ERROR) << "Failed to commit properties";
    ResetCurrentPlaneList(plane_list);
    return false;
  }

  plane_list->plane_list.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
  return true;
}

bool HardwareDisplayPlaneManagerAtomic::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const OverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  HardwareDisplayPlaneAtomic* atomic_plane =
      static_cast<HardwareDisplayPlaneAtomic*>(hw_plane);
  if (!atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(),
                                  crtc_id, overlay.buffer->GetFramebufferId(),
                                  overlay.display_bounds, src_rect)) {
    LOG(ERROR) << "Failed to set plane properties";
    return false;
  }
  atomic_plane->set_crtc(crtc);
  return true;
}

std::unique_ptr<HardwareDisplayPlane>
HardwareDisplayPlaneManagerAtomic::CreatePlane(uint32_t plane_id,
                                               uint32_t possible_crtcs) {
  return std::unique_ptr<HardwareDisplayPlane>(
      new HardwareDisplayPlaneAtomic(plane_id, possible_crtcs));
}

}  // namespace ui
