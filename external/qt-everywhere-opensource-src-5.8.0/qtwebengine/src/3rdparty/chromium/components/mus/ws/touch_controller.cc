// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/touch_controller.h"

#include <set>
#include <vector>

#include "base/logging.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_manager.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace mus {
namespace ws {

namespace {

// Computes the scale ratio for the TouchEvent's radius.
double ComputeTouchResolutionScale(const Display* touch_display,
                                   const ui::TouchscreenDevice& touch_device) {
  gfx::Size touch_display_size = touch_display->GetSize();

  if (touch_device.size.IsEmpty() || touch_display_size.IsEmpty())
    return 1.0;

  double display_area = touch_display_size.GetArea();
  double touch_area = touch_device.size.GetArea();
  double ratio = std::sqrt(display_area / touch_area);

  return ratio;
}

// Computes a touch transform that maps from window bounds to touchscreen size.
// Assumes scale factor of 1.0.
gfx::Transform ComputeTouchTransform(
    const Display* touch_display,
    const ui::TouchscreenDevice& touch_device) {
  gfx::Size touch_display_size = touch_display->GetSize();

  gfx::SizeF current_size(touch_display_size);
  gfx::SizeF touch_area(touch_device.size);
  gfx::Transform transform;

  if (current_size.IsEmpty() || touch_area.IsEmpty())
    return transform;

  // Take care of scaling between touchscreen area and display resolution.
  transform.Scale(current_size.width() / touch_area.width(),
                  current_size.height() / touch_area.height());
  return transform;
}

}  // namespace

TouchController::TouchController(DisplayManager* display_manager)
    : display_manager_(display_manager) {
  DCHECK(display_manager_);
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

TouchController::~TouchController() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

void TouchController::UpdateTouchTransforms() const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  device_manager->ClearTouchDeviceAssociations();

  const std::set<Display*>& touch_displays = display_manager_->displays();
  const std::vector<ui::TouchscreenDevice>& touch_devices =
      device_manager->GetTouchscreenDevices();

  // Mash can only handle a single display so this doesn't implement support for
  // matching touchscreens with multiple displays.
  // TODO(kylechar): Implement support for multiple displays when needed.
  if (touch_displays.size() == 1 && touch_devices.size() == 1) {
    const Display* touch_display = *touch_displays.begin();
    const ui::TouchscreenDevice& touch_device = touch_devices[0];

    int64_t touch_display_id = touch_display->GetPlatformDisplayId();
    int touch_device_id = touch_device.id;

    if (touch_device_id != ui::InputDevice::kInvalidId) {
      device_manager->UpdateTouchRadiusScale(
          touch_device_id,
          ComputeTouchResolutionScale(touch_display, touch_device));

      device_manager->UpdateTouchInfoForDisplay(
          touch_display_id, touch_device_id,
          ComputeTouchTransform(touch_display, touch_device));
    }
  }
}

void TouchController::OnTouchscreenDeviceConfigurationChanged() {
  UpdateTouchTransforms();
}

}  // namespace ws
}  // namespace mus
