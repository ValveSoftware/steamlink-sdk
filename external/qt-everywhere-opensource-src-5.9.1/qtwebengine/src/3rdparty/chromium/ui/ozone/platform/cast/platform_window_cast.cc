// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/platform_window_cast.h"

#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

PlatformWindowCast::PlatformWindowCast(PlatformWindowDelegate* delegate,
                                       const gfx::Rect& bounds)
    : delegate_(delegate), bounds_(bounds) {
  widget_ = (bounds.width() << 16) + bounds.height();
  delegate_->OnAcceleratedWidgetAvailable(widget_, 1.f);
}

gfx::Rect PlatformWindowCast::GetBounds() {
  return bounds_;
}

void PlatformWindowCast::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

void PlatformWindowCast::SetTitle(const base::string16& title) {
}

PlatformImeController* PlatformWindowCast::GetPlatformImeController() {
  return nullptr;
}

}  // namespace ui
