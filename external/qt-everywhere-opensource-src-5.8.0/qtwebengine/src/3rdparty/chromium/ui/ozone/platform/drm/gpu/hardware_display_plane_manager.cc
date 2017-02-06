// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

#include <drm_fourcc.h>

#include <set>
#include <utility>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {
namespace {

const float kFixedPointScaleValue = 65536.0f;

}  // namespace

HardwareDisplayPlaneList::HardwareDisplayPlaneList() {
#if defined(USE_DRM_ATOMIC)
  atomic_property_set.reset(drmModeAtomicAlloc());
#endif  // defined(USE_DRM_ATOMIC)
}

HardwareDisplayPlaneList::~HardwareDisplayPlaneList() {
}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(uint32_t crtc_id,
                                                     uint32_t framebuffer,
                                                     CrtcController* crtc)
    : crtc_id(crtc_id), framebuffer(framebuffer), crtc(crtc) {
}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(
    const PageFlipInfo& other) = default;

HardwareDisplayPlaneList::PageFlipInfo::~PageFlipInfo() {
}

HardwareDisplayPlaneList::PageFlipInfo::Plane::Plane(int plane,
                                                     int framebuffer,
                                                     const gfx::Rect& bounds,
                                                     const gfx::Rect& src_rect)
    : plane(plane),
      framebuffer(framebuffer),
      bounds(bounds),
      src_rect(src_rect) {
}

HardwareDisplayPlaneList::PageFlipInfo::Plane::~Plane() {
}

HardwareDisplayPlaneManager::HardwareDisplayPlaneManager() : drm_(nullptr) {
}

HardwareDisplayPlaneManager::~HardwareDisplayPlaneManager() {
}

bool HardwareDisplayPlaneManager::Initialize(DrmDevice* drm) {
  drm_ = drm;

  // Try to get all of the planes if possible, so we don't have to try to
  // discover hidden primary planes.
  bool has_universal_planes = false;
#if defined(DRM_CLIENT_CAP_UNIVERSAL_PLANES)
  has_universal_planes = drm->SetCapability(DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
#endif  // defined(DRM_CLIENT_CAP_UNIVERSAL_PLANES)

  ScopedDrmResourcesPtr resources(drmModeGetResources(drm->get_fd()));
  if (!resources) {
    PLOG(ERROR) << "Failed to get resources";
    return false;
  }

  ScopedDrmPlaneResPtr plane_resources(drmModeGetPlaneResources(drm->get_fd()));
  if (!plane_resources) {
    PLOG(ERROR) << "Failed to get plane resources";
    return false;
  }

  crtcs_.clear();
  for (int i = 0; i < resources->count_crtcs; ++i) {
    crtcs_.push_back(resources->crtcs[i]);
  }

  uint32_t num_planes = plane_resources->count_planes;
  std::set<uint32_t> plane_ids;
  for (uint32_t i = 0; i < num_planes; ++i) {
    ScopedDrmPlanePtr drm_plane(
        drmModeGetPlane(drm->get_fd(), plane_resources->planes[i]));
    if (!drm_plane) {
      PLOG(ERROR) << "Failed to get plane " << i;
      return false;
    }

    uint32_t formats_size = drm_plane->count_formats;
    plane_ids.insert(drm_plane->plane_id);
    std::unique_ptr<HardwareDisplayPlane> plane(
        CreatePlane(drm_plane->plane_id, drm_plane->possible_crtcs));

    std::vector<uint32_t> supported_formats(formats_size);
    for (uint32_t j = 0; j < formats_size; j++)
      supported_formats[j] = drm_plane->formats[j];

    if (plane->Initialize(drm, supported_formats, false, false)) {
      // CRTC controllers always assume they have a cursor plane and the cursor
      // plane is updated via cursor specific DRM API. Hence, we dont keep
      // track of Cursor plane here to avoid re-using it for any other purpose.
      if (plane->type() != HardwareDisplayPlane::kCursor)
        planes_.push_back(std::move(plane));
    }
  }

  // crbug.com/464085: if driver reports no primary planes for a crtc, create a
  // dummy plane for which we can assign exactly one overlay.
  // TODO(dnicoara): refactor this to simplify AssignOverlayPlanes and move
  // this workaround into HardwareDisplayPlaneLegacy.
  if (!has_universal_planes) {
    for (int i = 0; i < resources->count_crtcs; ++i) {
      if (plane_ids.find(resources->crtcs[i] - 1) == plane_ids.end()) {
        std::unique_ptr<HardwareDisplayPlane> dummy_plane(
            CreatePlane(resources->crtcs[i] - 1, (1 << i)));
        if (dummy_plane->Initialize(drm, std::vector<uint32_t>(), true,
                                    false)) {
          planes_.push_back(std::move(dummy_plane));
        }
      }
    }
  }

  std::sort(planes_.begin(), planes_.end(),
            [](const std::unique_ptr<HardwareDisplayPlane>& l,
               const std::unique_ptr<HardwareDisplayPlane>& r) {
              return l->plane_id() < r->plane_id();
            });

  PopulateSupportedFormats();
  return true;
}

std::unique_ptr<HardwareDisplayPlane> HardwareDisplayPlaneManager::CreatePlane(
    uint32_t plane_id,
    uint32_t possible_crtcs) {
  return std::unique_ptr<HardwareDisplayPlane>(
      new HardwareDisplayPlane(plane_id, possible_crtcs));
}

HardwareDisplayPlane* HardwareDisplayPlaneManager::FindNextUnusedPlane(
    size_t* index,
    uint32_t crtc_index,
    const OverlayPlane& overlay) const {
  for (size_t i = *index; i < planes_.size(); ++i) {
    auto plane = planes_[i].get();
    if (!plane->in_use() && IsCompatible(plane, overlay, crtc_index)) {
      *index = i + 1;
      return plane;
    }
  }
  return nullptr;
}

int HardwareDisplayPlaneManager::LookupCrtcIndex(uint32_t crtc_id) const {
  for (size_t i = 0; i < crtcs_.size(); ++i)
    if (crtcs_[i] == crtc_id)
      return i;
  return -1;
}

bool HardwareDisplayPlaneManager::IsCompatible(HardwareDisplayPlane* plane,
                                               const OverlayPlane& overlay,
                                               uint32_t crtc_index) const {
  if (!plane->CanUseForCrtc(crtc_index))
    return false;

  if (!plane->IsSupportedFormat(overlay.buffer->GetFramebufferPixelFormat()))
    return false;

  // TODO(kalyank): We should check for z-order and any needed transformation
  // support. Driver doesn't expose any property to check for z-order, can we
  // rely on the sorting we do based on plane ids ?

  return true;
}

void HardwareDisplayPlaneManager::PopulateSupportedFormats() {
  std::set<uint32_t> supported_formats;

  for (const auto& plane : planes_) {
    const std::vector<uint32_t>& formats = plane->supported_formats();
    supported_formats.insert(formats.begin(), formats.end());
  }

  supported_formats_.reserve(supported_formats.size());
  supported_formats_.assign(supported_formats.begin(), supported_formats.end());
}

void HardwareDisplayPlaneManager::ResetCurrentPlaneList(
    HardwareDisplayPlaneList* plane_list) const {
  for (auto* hardware_plane : plane_list->plane_list) {
    hardware_plane->set_in_use(false);
    hardware_plane->set_owning_crtc(0);
  }

  plane_list->plane_list.clear();
  plane_list->legacy_page_flips.clear();
#if defined(USE_DRM_ATOMIC)
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
#endif
}

void HardwareDisplayPlaneManager::BeginFrame(
    HardwareDisplayPlaneList* plane_list) {
  for (auto* plane : plane_list->old_plane_list) {
    plane->set_in_use(false);
  }
}

bool HardwareDisplayPlaneManager::AssignOverlayPlanes(
    HardwareDisplayPlaneList* plane_list,
    const OverlayPlaneList& overlay_list,
    uint32_t crtc_id,
    CrtcController* crtc) {
  int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Cannot find crtc " << crtc_id;
    return false;
  }

  size_t plane_idx = 0;
  HardwareDisplayPlane* primary_plane = nullptr;
  gfx::Rect primary_display_bounds;
  uint32_t primary_format;
  for (const auto& plane : overlay_list) {
    HardwareDisplayPlane* hw_plane =
        FindNextUnusedPlane(&plane_idx, crtc_index, plane);
    if (!hw_plane) {
      LOG(ERROR) << "Failed to find a free plane for crtc " << crtc_id;
      ResetCurrentPlaneList(plane_list);
      return false;
    }

    gfx::Rect fixed_point_rect;
    uint32_t fourcc_format = plane.buffer->GetFramebufferPixelFormat();
    if (hw_plane->type() != HardwareDisplayPlane::kDummy) {
      const gfx::Size& size = plane.buffer->GetSize();
      gfx::RectF crop_rect = plane.crop_rect;
      crop_rect.Scale(size.width(), size.height());

      // This returns a number in 16.16 fixed point, required by the DRM overlay
      // APIs.
      auto to_fixed_point =
          [](double v) -> uint32_t { return v * kFixedPointScaleValue; };
      fixed_point_rect = gfx::Rect(to_fixed_point(crop_rect.x()),
                                   to_fixed_point(crop_rect.y()),
                                   to_fixed_point(crop_rect.width()),
                                   to_fixed_point(crop_rect.height()));
    }

    // If Overlay completely covers primary and isn't transparent, than use
    // it as primary. This reduces the no of planes which need to be read in
    // display controller side.
    if (primary_plane) {
      bool needs_blending = true;
      if (fourcc_format == DRM_FORMAT_XRGB8888)
        needs_blending = false;
      // TODO(kalyank): Check if we can move this optimization to
      // DrmOverlayCandidatesHost.
      if (!needs_blending && primary_format == fourcc_format &&
          primary_display_bounds == plane.display_bounds) {
        ResetCurrentPlaneList(plane_list);
        hw_plane = primary_plane;
      }
    } else {
      primary_plane = hw_plane;
      primary_display_bounds = plane.display_bounds;
      primary_format = fourcc_format;
    }

    if (!SetPlaneData(plane_list, hw_plane, plane, crtc_id, fixed_point_rect,
                      crtc)) {
      ResetCurrentPlaneList(plane_list);
      return false;
    }

    plane_list->plane_list.push_back(hw_plane);
    hw_plane->set_owning_crtc(crtc_id);
    hw_plane->set_in_use(true);
  }
  return true;
}

const std::vector<uint32_t>& HardwareDisplayPlaneManager::GetSupportedFormats()
    const {
  return supported_formats_;
}

bool HardwareDisplayPlaneManager::IsFormatSupported(uint32_t fourcc_format,
                                                    uint32_t z_order,
                                                    uint32_t crtc_id) const {
  bool format_supported = false;
  int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Cannot find crtc " << crtc_id;
    return format_supported;
  }

  // We dont have a way to query z_order of a plane. This is a temporary
  // solution till driver exposes z_order property.
  uint32_t plane_z_order = 0;
  for (const auto& hardware_plane : planes_) {
    if (plane_z_order > z_order)
      break;

    if (!hardware_plane->CanUseForCrtc(crtc_index))
      continue;

    if (plane_z_order == z_order) {
      if (hardware_plane->IsSupportedFormat(fourcc_format))
        format_supported = true;

      break;
    } else {
      plane_z_order++;
    }
  }

  return format_supported;
}

}  // namespace ui
