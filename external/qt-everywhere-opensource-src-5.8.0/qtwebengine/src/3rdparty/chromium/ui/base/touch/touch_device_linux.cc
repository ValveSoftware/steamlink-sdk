// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

#include "base/logging.h"
#include "ui/events/devices/input_device_manager.h"

namespace ui {

namespace {

bool IsTouchDevicePresent() {
  return !InputDeviceManager::GetInstance()->GetTouchscreenDevices().empty();
}

}  // namespace

TouchScreensAvailability GetTouchScreensAvailability() {
  if (!IsTouchDevicePresent())
    return TouchScreensAvailability::NONE;

  return InputDeviceManager::GetInstance()->AreTouchscreensEnabled()
             ? TouchScreensAvailability::ENABLED
             : TouchScreensAvailability::DISABLED;
}

int MaxTouchPoints() {
  int max_touch = 0;
  const std::vector<ui::TouchscreenDevice>& touchscreen_devices =
      ui::InputDeviceManager::GetInstance()->GetTouchscreenDevices();
  for (const ui::TouchscreenDevice& device : touchscreen_devices) {
    if (device.touch_points > max_touch)
      max_touch = device.touch_points;
  }
  return max_touch;
}

// TODO(mustaq@chromium.org): Use mouse detection logic. crbug.com/495634
int GetAvailablePointerTypes() {
  // Assume a mouse is there
  int available_pointer_types = POINTER_TYPE_FINE;
  if (IsTouchDevicePresent())
    available_pointer_types |= POINTER_TYPE_COARSE;

  DCHECK(available_pointer_types);
  return available_pointer_types;
}

PointerType GetPrimaryPointerType() {
  int available_pointer_types = GetAvailablePointerTypes();
  if (available_pointer_types & POINTER_TYPE_FINE)
    return POINTER_TYPE_FINE;
  if (available_pointer_types & POINTER_TYPE_COARSE)
    return POINTER_TYPE_COARSE;
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

// TODO(mustaq@chromium.org): Use mouse detection logic. crbug.com/495634
int GetAvailableHoverTypes() {
  // Assume a mouse is there
  int available_hover_types = HOVER_TYPE_HOVER;
  if (IsTouchDevicePresent())
    available_hover_types |= HOVER_TYPE_ON_DEMAND;

  DCHECK(available_hover_types);
  return available_hover_types;
}

HoverType GetPrimaryHoverType() {
  int available_hover_types = GetAvailableHoverTypes();
  if (available_hover_types & HOVER_TYPE_HOVER)
    return HOVER_TYPE_HOVER;
  if (available_hover_types & HOVER_TYPE_ON_DEMAND)
    return HOVER_TYPE_ON_DEMAND;
  DCHECK_EQ(available_hover_types, HOVER_TYPE_NONE);
  return HOVER_TYPE_NONE;
}

}  // namespace ui
