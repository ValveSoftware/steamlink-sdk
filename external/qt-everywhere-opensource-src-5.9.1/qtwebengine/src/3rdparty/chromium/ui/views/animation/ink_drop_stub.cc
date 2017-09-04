// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_stub.h"

namespace views {

InkDropStub::InkDropStub() {}

InkDropStub::~InkDropStub() {}

void InkDropStub::HostSizeChanged(const gfx::Size& new_size) {}

InkDropState InkDropStub::GetTargetInkDropState() const {
  return InkDropState::HIDDEN;
}

void InkDropStub::AnimateToState(InkDropState state) {}

void InkDropStub::SnapToActivated() {}

void InkDropStub::SetHovered(bool is_hovered) {}

void InkDropStub::SetFocused(bool is_hovered) {}

}  // namespace views
