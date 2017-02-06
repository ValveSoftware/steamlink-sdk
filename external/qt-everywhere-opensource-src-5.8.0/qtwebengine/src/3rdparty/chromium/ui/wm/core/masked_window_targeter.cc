// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/masked_window_targeter.h"

#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/path.h"

namespace wm {

MaskedWindowTargeter::MaskedWindowTargeter(aura::Window* masked_window)
    : masked_window_(masked_window) {
}

MaskedWindowTargeter::~MaskedWindowTargeter() {}

bool MaskedWindowTargeter::EventLocationInsideBounds(
    aura::Window* window,
    const ui::LocatedEvent& event) const {
  if (window == masked_window_) {
    gfx::Path mask;
    if (!GetHitTestMask(window, &mask))
      return WindowTargeter::EventLocationInsideBounds(window, event);

    gfx::Size size = window->bounds().size();
    SkRegion clip_region;
    clip_region.setRect(0, 0, size.width(), size.height());

    gfx::Point point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window, &point);

    SkRegion mask_region;
    return mask_region.setPath(mask, clip_region) &&
           mask_region.contains(point.x(), point.y());
  }

  return WindowTargeter::EventLocationInsideBounds(window, event);
}

}  // namespace wm
