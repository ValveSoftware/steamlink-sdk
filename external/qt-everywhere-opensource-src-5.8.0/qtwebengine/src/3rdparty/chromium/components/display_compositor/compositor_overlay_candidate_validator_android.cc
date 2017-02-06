// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/compositor_overlay_candidate_validator_android.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/output/overlay_processor.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace display_compositor {

CompositorOverlayCandidateValidatorAndroid::
    CompositorOverlayCandidateValidatorAndroid() {}

CompositorOverlayCandidateValidatorAndroid::
    ~CompositorOverlayCandidateValidatorAndroid() {}

void CompositorOverlayCandidateValidatorAndroid::GetStrategies(
    cc::OverlayProcessor::StrategyList* strategies) {
  strategies->push_back(
      base::WrapUnique(new cc::OverlayStrategyUnderlay(this)));
}

void CompositorOverlayCandidateValidatorAndroid::CheckOverlaySupport(
    cc::OverlayCandidateList* candidates) {
  // There should only be at most a single overlay candidate: the video quad.
  // There's no check that the presented candidate is really a video frame for
  // a fullscreen video. Instead it's assumed that if a quad is marked as
  // overlayable, it's a fullscreen video quad.
  DCHECK_LE(candidates->size(), 1u);

  if (!candidates->empty()) {
    cc::OverlayCandidate& candidate = candidates->front();
    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));
    candidate.overlay_handled = true;
    candidate.plane_z_order = -1;
  }
}

bool CompositorOverlayCandidateValidatorAndroid::AllowCALayerOverlays() {
  return false;
}

// Overlays will still be allowed when software mirroring is enabled, even
// though they won't appear in the mirror.
void CompositorOverlayCandidateValidatorAndroid::SetSoftwareMirrorMode(
    bool enabled) {}

}  // namespace display_compositor
