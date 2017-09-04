// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"
#include "base/logging.h"
#include "base/win/windows_version.h"
#include <windows.h>

namespace ui {

namespace {

bool IsTouchDevicePresent() {
  int value = GetSystemMetrics(SM_DIGITIZER);
  return (value & NID_READY) &&
      ((value & NID_INTEGRATED_TOUCH) || (value & NID_EXTERNAL_TOUCH));
}

int GetAvailablePointerTypes() {
  int available_pointer_types = 0;
  if (IsTouchDevicePresent())
    available_pointer_types |= POINTER_TYPE_COARSE;
  if (GetSystemMetrics(SM_MOUSEPRESENT) != 0 &&
      GetSystemMetrics(SM_CMOUSEBUTTONS) > 0)
    available_pointer_types |= POINTER_TYPE_FINE;

  if (available_pointer_types == 0)
    available_pointer_types = POINTER_TYPE_NONE;

  return available_pointer_types;
}

int GetAvailableHoverTypes() {
  int available_hover_types = 0;
  if (IsTouchDevicePresent())
    available_hover_types |= HOVER_TYPE_ON_DEMAND;
  if (GetSystemMetrics(SM_MOUSEPRESENT) != 0)
    available_hover_types |= HOVER_TYPE_HOVER;

  if (available_hover_types == 0)
    available_hover_types = HOVER_TYPE_NONE;

  return available_hover_types;
}

}  // namespace

TouchScreensAvailability GetTouchScreensAvailability() {
  if (!IsTouchDevicePresent())
    return TouchScreensAvailability::NONE;

  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  if (!IsTouchDevicePresent())
    return 0;

  return GetSystemMetrics(SM_MAXIMUMTOUCHES);
}

PointerType GetPrimaryPointerType(int available_pointer_types) {
  if (available_pointer_types & POINTER_TYPE_FINE)
    return POINTER_TYPE_FINE;
  if (available_pointer_types & POINTER_TYPE_COARSE)
    return POINTER_TYPE_COARSE;
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

HoverType GetPrimaryHoverType(int available_hover_types) {
  if (available_hover_types & HOVER_TYPE_HOVER)
    return HOVER_TYPE_HOVER;
  if (available_hover_types & HOVER_TYPE_ON_DEMAND)
    return HOVER_TYPE_ON_DEMAND;
  DCHECK_EQ(available_hover_types, HOVER_TYPE_NONE);
  return HOVER_TYPE_NONE;
}

std::pair<int, int> GetAvailablePointerAndHoverTypes() {
  return std::make_pair(GetAvailablePointerTypes(), GetAvailableHoverTypes());
}

}  // namespace ui
