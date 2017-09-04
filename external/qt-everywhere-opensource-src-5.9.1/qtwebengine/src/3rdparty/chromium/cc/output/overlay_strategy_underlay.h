// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_STRATEGY_UNDERLAY_H_
#define CC_OUTPUT_OVERLAY_STRATEGY_UNDERLAY_H_

#include "base/macros.h"
#include "cc/output/overlay_processor.h"

namespace cc {

class OverlayCandidateValidator;

// The underlay strategy looks for a video quad without regard to quads above
// it. The video is "underlaid" through a black transparent quad substituted
// for the video quad. The overlay content can then be blended in by the
// hardware under the the scene. This is only valid for overlay contents that
// are fully opaque.
class CC_EXPORT OverlayStrategyUnderlay : public OverlayProcessor::Strategy {
 public:
  explicit OverlayStrategyUnderlay(
      OverlayCandidateValidator* capability_checker);
  ~OverlayStrategyUnderlay() override;

  bool Attempt(ResourceProvider* resource_provider,
               RenderPass* render_pass,
               OverlayCandidateList* candidate_list) override;

 private:
  OverlayCandidateValidator* capability_checker_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyUnderlay);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_STRATEGY_UNDERLAY_H_
