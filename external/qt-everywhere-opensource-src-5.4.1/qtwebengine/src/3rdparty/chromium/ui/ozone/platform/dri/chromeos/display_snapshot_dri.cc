// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/chromeos/display_snapshot_dri.h"

#include <stdint.h>
#include <stdlib.h>
#include <xf86drmMode.h>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/display/util/edid_parser.h"
#include "ui/ozone/platform/dri/chromeos/display_mode_dri.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

namespace {

DisplayConnectionType GetDisplayType(drmModeConnector* connector) {
  switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_VGA:
      return DISPLAY_CONNECTION_TYPE_VGA;
    case DRM_MODE_CONNECTOR_DVII:
    case DRM_MODE_CONNECTOR_DVID:
    case DRM_MODE_CONNECTOR_DVIA:
      return DISPLAY_CONNECTION_TYPE_DVI;
    case DRM_MODE_CONNECTOR_LVDS:
    case DRM_MODE_CONNECTOR_eDP:
      return DISPLAY_CONNECTION_TYPE_INTERNAL;
    case DRM_MODE_CONNECTOR_DisplayPort:
      return DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
      return DISPLAY_CONNECTION_TYPE_HDMI;
    default:
      return DISPLAY_CONNECTION_TYPE_UNKNOWN;
  }
}

bool IsAspectPreserving(DriWrapper* drm, drmModeConnector* connector) {
  drmModePropertyRes* property = drm->GetProperty(connector, "scaling mode");
  if (property) {
    for (int j = 0; j < property->count_enums; ++j) {
      if (property->enums[j].value ==
              connector->prop_values[property->prop_id] &&
          strcmp(property->enums[j].name, "Full aspect") == 0) {
        drm->FreeProperty(property);
        return true;
      }
    }

    drm->FreeProperty(property);
  }

  return false;
}

}  // namespace

DisplaySnapshotDri::DisplaySnapshotDri(
    DriWrapper* drm,
    drmModeConnector* connector,
    drmModeCrtc* crtc,
    uint32_t index)
    : DisplaySnapshot(index,
                      false,
                      gfx::Point(crtc->x, crtc->y),
                      gfx::Size(connector->mmWidth, connector->mmHeight),
                      GetDisplayType(connector),
                      IsAspectPreserving(drm, connector),
                      false,
                      std::string(),
                      std::vector<const DisplayMode*>(),
                      NULL,
                      NULL),
      connector_(connector->connector_id),
      crtc_(crtc->crtc_id),
      dpms_property_(drm->GetProperty(connector, "DPMS")) {
  drmModePropertyBlobRes* edid_blob = drm->GetPropertyBlob(connector, "EDID");

  if (edid_blob) {
    std::vector<uint8_t> edid(
        static_cast<uint8_t*>(edid_blob->data),
        static_cast<uint8_t*>(edid_blob->data) + edid_blob->length);

    has_proper_display_id_ = GetDisplayIdFromEDID(edid, index, &display_id_);
    ParseOutputDeviceData(edid, NULL, &display_name_);
    ParseOutputOverscanFlag(edid, &overscan_flag_);

    drm->FreePropertyBlob(edid_blob);
  } else {
    VLOG(1) << "Failed to get EDID blob for connector "
            << connector->connector_id;
  }

  for (int i = 0; i < connector->count_modes; ++i) {
    drmModeModeInfo& mode = connector->modes[i];
    modes_.push_back(new DisplayModeDri(mode));

    if (crtc->mode_valid && SameMode(crtc->mode, mode))
      current_mode_ = modes_.back();

    if (mode.type & DRM_MODE_TYPE_PREFERRED)
      native_mode_ = modes_.back();
  }
}

DisplaySnapshotDri::~DisplaySnapshotDri() {
  if (dpms_property_)
    drmModeFreeProperty(dpms_property_);
}

std::string DisplaySnapshotDri::ToString() const {
  return base::StringPrintf(
      "[type=%d, connector=%" PRIu32 ", crtc=%" PRIu32 ", mode=%s, dim=%s]",
      type_,
      connector_,
      crtc_,
      current_mode_ ? current_mode_->ToString().c_str() : "NULL",
      physical_size_.ToString().c_str());
}

}  // namespace ui
