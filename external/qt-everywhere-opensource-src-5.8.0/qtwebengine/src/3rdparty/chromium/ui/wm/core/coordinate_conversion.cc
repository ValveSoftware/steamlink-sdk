// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/coordinate_conversion.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/gfx/geometry/point.h"

namespace wm {

void ConvertPointToScreen(const aura::Window* window, gfx::Point* point) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  DCHECK(aura::client::GetScreenPositionClient(window->GetRootWindow()));
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointToScreen(window, point);
}

void ConvertPointFromScreen(const aura::Window* window,
                            gfx::Point* point_in_screen) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  DCHECK(aura::client::GetScreenPositionClient(window->GetRootWindow()));
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointFromScreen(window, point_in_screen);
}

}  // namespace wm
