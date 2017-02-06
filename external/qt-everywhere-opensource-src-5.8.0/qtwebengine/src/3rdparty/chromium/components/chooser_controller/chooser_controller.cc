// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chooser_controller/chooser_controller.h"

#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

ChooserController::ChooserController(content::RenderFrameHost* owner)
    : owning_frame_(owner) {}

ChooserController::~ChooserController() {}

url::Origin ChooserController::GetOrigin() const {
  return owning_frame_ ? owning_frame_->GetLastCommittedOrigin()
                       : url::Origin();
}
