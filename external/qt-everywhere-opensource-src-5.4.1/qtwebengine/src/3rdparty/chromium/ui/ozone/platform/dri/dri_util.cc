// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_util.h"

#include <stdint.h>
#include <stdlib.h>
#include <xf86drmMode.h>

namespace ui {

namespace {

bool IsCrtcInUse(uint32_t crtc,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  for (size_t i = 0; i < displays.size(); ++i) {
    if (crtc == displays[i]->crtc()->crtc_id)
      return true;
  }

  return false;
}

uint32_t GetCrtc(int fd,
                 drmModeConnector* connector,
                 drmModeRes* resources,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  // If the connector already has an encoder try to re-use.
  if (connector->encoder_id) {
    drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoder_id);
    if (encoder) {
      if (encoder->crtc_id && !IsCrtcInUse(encoder->crtc_id, displays)) {
        uint32_t crtc = encoder->crtc_id;
        drmModeFreeEncoder(encoder);
        return crtc;
      }
      drmModeFreeEncoder(encoder);
    }
  }

  // Try to find an encoder for the connector.
  for (int i = 0; i < connector->count_encoders; ++i) {
    drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoders[i]);
    if (!encoder)
      continue;

    for (int j = 0; j < resources->count_crtcs; ++j) {
      // Check if the encoder is compatible with this CRTC
      if (!(encoder->possible_crtcs & (1 << j)) ||
          IsCrtcInUse(resources->crtcs[j], displays)) {
        continue;
      }

      drmModeFreeEncoder(encoder);
      return resources->crtcs[j];
    }

    drmModeFreeEncoder(encoder);
  }

  return 0;
}

}  // namespace

HardwareDisplayControllerInfo::HardwareDisplayControllerInfo(
    drmModeConnector* connector,
    drmModeCrtc* crtc)
    : connector_(connector),
      crtc_(crtc) {}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() {
  drmModeFreeConnector(connector_);
  drmModeFreeCrtc(crtc_);
}

ScopedVector<HardwareDisplayControllerInfo>
GetAvailableDisplayControllerInfos(int fd, drmModeRes* resources) {
  ScopedVector<HardwareDisplayControllerInfo> displays;

  for (int i = 0; i < resources->count_connectors; ++i) {
    drmModeConnector* connector = drmModeGetConnector(
        fd, resources->connectors[i]);

    if (!connector)
      continue;

    if (connector->connection != DRM_MODE_CONNECTED ||
        connector->count_modes == 0) {
      drmModeFreeConnector(connector);
      continue;
    }

    uint32_t crtc_id = GetCrtc(fd, connector, resources, displays);
    if (!crtc_id) {
      drmModeFreeConnector(connector);
      continue;
    }

    drmModeCrtc* crtc = drmModeGetCrtc(fd, crtc_id);
    displays.push_back(new HardwareDisplayControllerInfo(connector, crtc));
  }

  return displays.Pass();
}

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs) {
  return lhs.clock == rhs.clock &&
         lhs.hdisplay == rhs.hdisplay &&
         lhs.vdisplay == rhs.vdisplay &&
         lhs.vrefresh == rhs.vrefresh &&
         lhs.hsync_start == rhs.hsync_start &&
         lhs.hsync_end == rhs.hsync_end &&
         lhs.htotal == rhs.htotal &&
         lhs.hskew == rhs.hskew &&
         lhs.vsync_start == rhs.vsync_start &&
         lhs.vsync_end == rhs.vsync_end &&
         lhs.vtotal == rhs.vtotal &&
         lhs.vscan == rhs.vscan &&
         lhs.flags == rhs.flags &&
         strcmp(lhs.name, rhs.name) == 0;
}

}  // namespace ui
