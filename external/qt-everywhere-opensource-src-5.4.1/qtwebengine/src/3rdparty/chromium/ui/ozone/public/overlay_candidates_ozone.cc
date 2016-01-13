// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/overlay_candidates_ozone.h"

#include <stdlib.h>

namespace ui {

OverlayCandidatesOzone::OverlaySurfaceCandidate::OverlaySurfaceCandidate()
    : transform(gfx::OVERLAY_TRANSFORM_NONE),
      format(SurfaceFactoryOzone::UNKNOWN),
      overlay_handled(false) {
}

OverlayCandidatesOzone::OverlaySurfaceCandidate::~OverlaySurfaceCandidate() {
}

void OverlayCandidatesOzone::CheckOverlaySupport(
    OverlaySurfaceCandidateList* surfaces) {
  NOTREACHED();
}

OverlayCandidatesOzone::~OverlayCandidatesOzone() {
}

}  // namespace ui
