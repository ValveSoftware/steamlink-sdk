// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_MONITOR_WIN_H_
#define UI_VIEWS_WIDGET_MONITOR_WIN_H_

#include "ui/views/views_export.h"

namespace gfx {
class Rect;
}

namespace views {

// Returns the bounds for the monitor that contains the largest area of
// intersection with the specified rectangle.
VIEWS_EXPORT gfx::Rect GetMonitorBoundsForRect(const gfx::Rect& rect);

}  // namespace views

#endif  // UI_VIEWS_WIDGET_MONITOR_WIN_H_
