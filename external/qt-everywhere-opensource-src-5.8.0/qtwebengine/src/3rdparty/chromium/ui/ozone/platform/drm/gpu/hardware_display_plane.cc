// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

#include <drm_fourcc.h>

#include "base/logging.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

#ifndef DRM_PLANE_TYPE_OVERLAY
#define DRM_PLANE_TYPE_OVERLAY 0
#endif

#ifndef DRM_PLANE_TYPE_PRIMARY
#define DRM_PLANE_TYPE_PRIMARY 1
#endif

#ifndef DRM_PLANE_TYPE_CURSOR
#define DRM_PLANE_TYPE_CURSOR 2
#endif

namespace ui {

namespace {

const char* kTypePropName = "type";
HardwareDisplayPlane::Type GetPlaneType(int value) {
  switch (value) {
    case DRM_PLANE_TYPE_CURSOR:
      return HardwareDisplayPlane::kCursor;
    case DRM_PLANE_TYPE_PRIMARY:
      return HardwareDisplayPlane::kPrimary;
    case DRM_PLANE_TYPE_OVERLAY:
      return HardwareDisplayPlane::kOverlay;
    default:
      NOTREACHED();
      return HardwareDisplayPlane::kDummy;
  }
}

}  // namespace

HardwareDisplayPlane::HardwareDisplayPlane(uint32_t plane_id,
                                           uint32_t possible_crtcs)
    : plane_id_(plane_id), possible_crtcs_(possible_crtcs) {
}

HardwareDisplayPlane::~HardwareDisplayPlane() {
}

bool HardwareDisplayPlane::CanUseForCrtc(uint32_t crtc_index) {
  return possible_crtcs_ & (1 << crtc_index);
}

bool HardwareDisplayPlane::Initialize(DrmDevice* drm,
                                      const std::vector<uint32_t>& formats,
                                      bool is_dummy,
                                      bool test_only) {
  supported_formats_ = formats;

  if (test_only)
    return true;

  if (is_dummy) {
    type_ = kDummy;
    supported_formats_.push_back(DRM_FORMAT_XRGB8888);
    return true;
  }

  ScopedDrmObjectPropertyPtr plane_props(drmModeObjectGetProperties(
      drm->get_fd(), plane_id_, DRM_MODE_OBJECT_PLANE));
  if (!plane_props) {
    PLOG(ERROR) << "Unable to get plane properties.";
    return false;
  }

  uint32_t count_props = plane_props->count_props;
  for (uint32_t i = 0; i < count_props; i++) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(drm->get_fd(), plane_props->props[i]));
    if (property && !strcmp(property->name, kTypePropName)) {
      type_ = GetPlaneType(plane_props->prop_values[i]);
      break;
    }
  }

  return InitializeProperties(drm, plane_props);
}

bool HardwareDisplayPlane::IsSupportedFormat(uint32_t format) {
  if (!format)
    return false;

  if (last_used_format_ == format)
    return true;

  for (auto& element : supported_formats_) {
    if (element == format) {
      last_used_format_ = format;
      return true;
    }
  }

  last_used_format_ = 0;
  return false;
}

const std::vector<uint32_t>& HardwareDisplayPlane::supported_formats() const {
  return supported_formats_;
}

bool HardwareDisplayPlane::InitializeProperties(
    DrmDevice* drm,
    const ScopedDrmObjectPropertyPtr& plane_props) {
  return true;
}

}  // namespace ui
