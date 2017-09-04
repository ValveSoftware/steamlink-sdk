// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
#define CC_OUTPUT_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_

#include "base/macros.h"
#include "cc/output/overlay_processor.h"

namespace cc {

class OverlayCandidateValidator;

class CC_EXPORT OverlayStrategySingleOnTop : public OverlayProcessor::Strategy {
 public:
  explicit OverlayStrategySingleOnTop(
      OverlayCandidateValidator* capability_checker);
  ~OverlayStrategySingleOnTop() override;

  bool Attempt(ResourceProvider* resource_provider,
               RenderPass* render_pass,
               OverlayCandidateList* candidate_list) override;

 private:
  bool TryOverlay(QuadList* quad_list,
                  OverlayCandidateList* candidate_list,
                  const OverlayCandidate& candidate,
                  QuadList::Iterator candidate_iterator);

  OverlayCandidateValidator* capability_checker_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategySingleOnTop);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
