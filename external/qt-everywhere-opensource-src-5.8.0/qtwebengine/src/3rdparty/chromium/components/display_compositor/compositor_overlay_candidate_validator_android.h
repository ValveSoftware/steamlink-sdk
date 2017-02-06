// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_

#include "base/macros.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "components/display_compositor/display_compositor_export.h"

namespace display_compositor {

// An overlay validator for supporting fullscreen video underlays on Android.
// Things are a bit different on Android compared with other platforms. By the
// time a video frame is marked as overlayable it means the video decoder was
// outputting to a Surface that we can't read back from. As a result, the
// overlay must always succeed, or the video won't be visible. This is one of of
// the reasons that only fullscreen is supported: we have to be sure that
// nothing will cause the overlay to be rejected, because there's no fallback to
// gl compositing.
class DISPLAY_COMPOSITOR_EXPORT CompositorOverlayCandidateValidatorAndroid
    : public CompositorOverlayCandidateValidator {
 public:
  CompositorOverlayCandidateValidatorAndroid();
  ~CompositorOverlayCandidateValidatorAndroid() override;

  void GetStrategies(cc::OverlayProcessor::StrategyList* strategies) override;
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override;
  bool AllowCALayerOverlays() override;

  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorOverlayCandidateValidatorAndroid);
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_
