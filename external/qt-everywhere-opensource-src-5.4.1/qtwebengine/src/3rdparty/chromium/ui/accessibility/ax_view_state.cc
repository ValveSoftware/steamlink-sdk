// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_view_state.h"

namespace ui {

AXViewState::AXViewState()
    : role(AX_ROLE_CLIENT),
      selection_start(-1),
      selection_end(-1),
      index(-1),
      count(-1),
      state_(0) { }

AXViewState::~AXViewState() { }

void AXViewState::AddStateFlag(ui::AXState state_flag) {
  state_ |= (1 << state_flag);
}

bool AXViewState::HasStateFlag(ui::AXState state_flag) const {
  return 0 != (state_ & (1 << state_flag));
}

}  // namespace ui
