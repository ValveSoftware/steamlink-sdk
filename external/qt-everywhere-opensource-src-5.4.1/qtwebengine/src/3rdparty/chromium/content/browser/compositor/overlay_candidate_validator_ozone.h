// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
#define CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_

#include "cc/output/overlay_candidate_validator.h"

#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class OverlayCandidatesOzone;
}

namespace content {

class CONTENT_EXPORT OverlayCandidateValidatorOzone
    : public cc::OverlayCandidateValidator {
 public:
  OverlayCandidateValidatorOzone(
      gfx::AcceleratedWidget widget,
      ui::OverlayCandidatesOzone* overlay_candidates);
  virtual ~OverlayCandidateValidatorOzone();

  // cc::OverlayCandidateValidator implementation.
  virtual void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) OVERRIDE;

 private:
  gfx::AcceleratedWidget widget_;
  ui::OverlayCandidatesOzone* overlay_candidates_;

  DISALLOW_COPY_AND_ASSIGN(OverlayCandidateValidatorOzone);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
