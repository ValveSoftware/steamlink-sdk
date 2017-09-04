// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_VIEWPORT_METRICS_H_
#define SERVICES_UI_DISPLAY_VIEWPORT_METRICS_H_

#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {

struct ViewportMetrics {
  std::string ToString() const;

  gfx::Rect bounds;     // DIP.
  gfx::Rect work_area;  // DIP.
  gfx::Size pixel_size;
  Display::Rotation rotation = Display::ROTATE_0;
  float device_scale_factor = 0.0f;
};

inline bool operator==(const ViewportMetrics& lhs, const ViewportMetrics& rhs) {
  return lhs.bounds == rhs.bounds && lhs.work_area == rhs.work_area &&
         lhs.pixel_size == rhs.pixel_size && lhs.rotation == rhs.rotation &&
         lhs.device_scale_factor == rhs.device_scale_factor;
}

inline bool operator!=(const ViewportMetrics& lhs, const ViewportMetrics& rhs) {
  return !(lhs == rhs);
}

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_VIEWPORT_METRICS_H_
