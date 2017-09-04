// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_COORDINATE_CONVERSION_H_
#define UI_WM_CORE_COORDINATE_CONVERSION_H_

#include "ui/wm/wm_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
}  // namespace gfx

namespace wm {

// Converts the |point| from a given |window|'s coordinates into the screen
// coordinates.
WM_EXPORT void ConvertPointToScreen(const aura::Window* window,
                                    gfx::Point* point);

// Converts the |point| from the screen coordinates to a given |window|'s
// coordinates.
WM_EXPORT void ConvertPointFromScreen(const aura::Window* window,
                                      gfx::Point* point_in_screen);

}  // namespace wm

#endif  // UI_WM_CORE_COORDINATE_CONVERSION_H_
