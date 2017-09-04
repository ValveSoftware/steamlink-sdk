// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_FINDER_H_
#define UI_DISPLAY_DISPLAY_FINDER_H_

#include <vector>

#include "ui/display/display_export.h"

namespace gfx {
class Point;
class Rect;
}

namespace display {
class Display;

// Returns the display in |displays| closest to |point|.
DISPLAY_EXPORT const Display* FindDisplayNearestPoint(
    const std::vector<Display>& displays,
    const gfx::Point& point);

// Returns the display in |displays| with the biggest intersection of |rect|.
// If none of the displays intersect |rect| null is returned.
DISPLAY_EXPORT const Display* FindDisplayWithBiggestIntersection(
    const std::vector<Display>& displays,
    const gfx::Rect& rect);

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_FINDER_H_
