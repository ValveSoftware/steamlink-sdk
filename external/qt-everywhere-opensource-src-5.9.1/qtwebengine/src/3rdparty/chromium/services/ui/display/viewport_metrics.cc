// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/viewport_metrics.h"

#include "base/strings/stringprintf.h"

namespace display {

namespace {

std::string RotationString(Display::Rotation rotation) {
  switch (rotation) {
    case Display::ROTATE_0:
      return "0";
    case Display::ROTATE_90:
      return "90";
    case Display::ROTATE_180:
      return "180";
    case Display::ROTATE_270:
      return "270";
  }
  NOTREACHED();
  return "Invalid Rotation";
}

}  // namespace

std::string ViewportMetrics::ToString() const {
  return base::StringPrintf(
      "ViewportMetrics(bounds=%s, work_area=%s, pixel_size=%s, "
      "rotation=%s, device_scale_factor=%g)",
      bounds.ToString().c_str(), work_area.ToString().c_str(),
      pixel_size.ToString().c_str(), RotationString(rotation).c_str(),
      device_scale_factor);
}

}  // namespace display
