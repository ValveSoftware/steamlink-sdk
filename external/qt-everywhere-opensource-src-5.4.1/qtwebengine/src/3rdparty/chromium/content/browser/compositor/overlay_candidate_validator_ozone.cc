// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/overlay_candidate_validator_ozone.h"

#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace content {

static ui::SurfaceFactoryOzone::BufferFormat GetOzoneFormat(
    cc::ResourceFormat overlay_format) {
  switch (overlay_format) {
    case cc::RGBA_8888:
      return ui::SurfaceFactoryOzone::RGBA_8888;
    case cc::RGBA_4444:
    case cc::BGRA_8888:
    case cc::LUMINANCE_8:
    case cc::RGB_565:
    case cc::ETC1:
      break;
  }
  NOTREACHED();
  return ui::SurfaceFactoryOzone::UNKNOWN;
}

OverlayCandidateValidatorOzone::OverlayCandidateValidatorOzone(
    gfx::AcceleratedWidget widget,
    ui::OverlayCandidatesOzone* overlay_candidates)
    : widget_(widget), overlay_candidates_(overlay_candidates) {
}

OverlayCandidateValidatorOzone::~OverlayCandidateValidatorOzone() {}

void OverlayCandidateValidatorOzone::CheckOverlaySupport(
    cc::OverlayCandidateList* surfaces) {
  DCHECK_GE(2U, surfaces->size());
  ui::OverlayCandidatesOzone::OverlaySurfaceCandidateList ozone_surface_list;
  ozone_surface_list.resize(surfaces->size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    ozone_surface_list.at(i).transform = surfaces->at(i).transform;
    ozone_surface_list.at(i).format = GetOzoneFormat(surfaces->at(i).format);
    ozone_surface_list.at(i).display_rect = surfaces->at(i).display_rect;
    ozone_surface_list.at(i).crop_rect = surfaces->at(i).uv_rect;
    ozone_surface_list.at(i).plane_z_order = surfaces->at(i).plane_z_order;
  }

  overlay_candidates_->CheckOverlaySupport(&ozone_surface_list);
  DCHECK_EQ(surfaces->size(), ozone_surface_list.size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    surfaces->at(i).overlay_handled = ozone_surface_list.at(i).overlay_handled;
  }
}

}  // namespace content
