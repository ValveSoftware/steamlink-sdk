// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TOUCH_CONTROLLER_H_
#define COMPONENTS_MUS_WS_TOUCH_CONTROLLER_H_

#include "base/macros.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace mus {
namespace ws {

class DisplayManager;

// Tracks changes to displays and touchscreen devices. Updates the mapping
// between display devices and touchscreen devices when changes occur.
class TouchController : public ui::InputDeviceEventObserver {
 public:
  explicit TouchController(DisplayManager* display_manager);
  ~TouchController() override;

  void UpdateTouchTransforms() const;

  // ui::InputDeviceEventObserver:
  void OnTouchscreenDeviceConfigurationChanged() override;

 private:
  DisplayManager* display_manager_;

  DISALLOW_COPY_AND_ASSIGN(TouchController);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TOUCH_CONTROLLER_H_
