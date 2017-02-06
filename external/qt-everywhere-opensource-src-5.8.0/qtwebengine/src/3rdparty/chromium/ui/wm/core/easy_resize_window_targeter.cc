// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/easy_resize_window_targeter.h"

#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/transient_window_client.h"

namespace wm {

EasyResizeWindowTargeter::EasyResizeWindowTargeter(
    aura::Window* container,
    const gfx::Insets& mouse_extend,
    const gfx::Insets& touch_extend)
    : container_(container),
      mouse_extend_(mouse_extend),
      touch_extend_(touch_extend) {
}

EasyResizeWindowTargeter::~EasyResizeWindowTargeter() {
}

bool EasyResizeWindowTargeter::EventLocationInsideBounds(
    aura::Window* window,
    const ui::LocatedEvent& event) const {
  if (ShouldUseExtendedBounds(window)) {
    // Note that |event|'s location is in |window|'s parent's coordinate system,
    // so convert it to |window|'s coordinate system first.
    gfx::Point point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window, &point);

    gfx::Rect bounds(window->bounds().size());
    if (event.IsTouchEvent() || event.IsGestureEvent())
      bounds.Inset(touch_extend_);
    else
      bounds.Inset(mouse_extend_);

    return bounds.Contains(point);
  }
  return WindowTargeter::EventLocationInsideBounds(window, event);
}

bool EasyResizeWindowTargeter::ShouldUseExtendedBounds(
    const aura::Window* window) const {
  // Use the extended bounds only for immediate child windows of |container_|.
  // Use the default targeter otherwise.
  if (window->parent() != container_)
    return false;

  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  return !transient_window_client ||
      !transient_window_client->GetTransientParent(window) ||
      transient_window_client->GetTransientParent(window) == container_;
}

}  // namespace wm
