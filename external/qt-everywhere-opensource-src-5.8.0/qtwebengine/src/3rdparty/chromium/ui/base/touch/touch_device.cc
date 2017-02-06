// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

namespace ui {

// Platforms supporting touch link in an alternate implementation of this
// method.
TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::NONE;
}

int MaxTouchPoints() {
  return 0;
}

int GetAvailablePointerTypes() {
  // Assume a non-touch-device with a mouse
  return POINTER_TYPE_FINE;
}

PointerType GetPrimaryPointerType() {
  // Assume a non-touch-device with a mouse
  return POINTER_TYPE_FINE;
}

int GetAvailableHoverTypes() {
  // Assume a non-touch-device with a mouse
  return HOVER_TYPE_HOVER;
}

HoverType GetPrimaryHoverType() {
  // Assume a non-touch-device with a mouse
  return HOVER_TYPE_HOVER;
}

}  // namespace ui
