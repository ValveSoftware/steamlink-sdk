// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_screen.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace engine {

namespace {

const int64_t kDisplayId = 1;

}  // namespace

BlimpScreen::BlimpScreen() {
  display::Display display(kDisplayId);
  ProcessDisplayChanged(display, true /* is_primary */);
}

BlimpScreen::~BlimpScreen() {}

void BlimpScreen::UpdateDisplayScaleAndSize(float scale,
                                            const gfx::Size& size) {
  display::Display display(GetPrimaryDisplay());
  if (scale == display.device_scale_factor() &&
      size == display.GetSizeInPixel()) {
    return;
  }

  display.SetScaleAndBounds(scale, gfx::Rect(size));
  display_list().UpdateDisplay(display);
}

gfx::Point BlimpScreen::GetCursorScreenPoint() {
  return gfx::Point();
}

bool BlimpScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow BlimpScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return window_tree_host_
             ? window_tree_host_->window()->GetTopWindowContainingPoint(point)
             : gfx::NativeWindow(nullptr);
}

display::Display BlimpScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

}  // namespace engine
}  // namespace blimp
