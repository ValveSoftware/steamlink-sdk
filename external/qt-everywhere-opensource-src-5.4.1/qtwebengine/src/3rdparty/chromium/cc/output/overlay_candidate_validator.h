// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_CANDIDATE_VALIDATOR_H_
#define CC_OUTPUT_OVERLAY_CANDIDATE_VALIDATOR_H_

#include <vector>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/output/overlay_candidate.h"
#include "cc/resources/resource_format.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

// This class that can be used to answer questions about possible overlay
// configurations for a particular output device.
class CC_EXPORT OverlayCandidateValidator {
 public:
  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited.
  virtual void CheckOverlaySupport(OverlayCandidateList* surfaces) = 0;

  virtual ~OverlayCandidateValidator() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_CANDIDATE_VALIDATOR_H_
