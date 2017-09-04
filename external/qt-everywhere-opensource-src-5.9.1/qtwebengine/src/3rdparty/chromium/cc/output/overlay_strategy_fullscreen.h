// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_STRATEGY_FULLSCREEN_H_
#define CC_OUTPUT_OVERLAY_STRATEGY_FULLSCREEN_H_

#include "base/macros.h"
#include "cc/output/overlay_processor.h"

namespace cc {

class OverlayCandidateValidator;
// Overlay strategy to promote a single full screen quad to an overlay.
// The promoted quad should have all the property of the framebuffer and it
// should be possible to use it as such.
class CC_EXPORT OverlayStrategyFullscreen : public OverlayProcessor::Strategy {
 public:
  explicit OverlayStrategyFullscreen(
      OverlayCandidateValidator* capability_checker);
  ~OverlayStrategyFullscreen() override;

  bool Attempt(ResourceProvider* resource_provider,
               RenderPass* render_pass,
               OverlayCandidateList* candidate_list) override;

 private:
  OverlayCandidateValidator* capability_checker_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyFullscreen);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_STRATEGY_FULLSCREEN_H_
