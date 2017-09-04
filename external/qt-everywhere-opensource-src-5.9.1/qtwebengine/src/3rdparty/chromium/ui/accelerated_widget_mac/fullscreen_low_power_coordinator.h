// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_FULLSCREEN_LOW_POWER_COORDINATOR_H_
#define UI_ACCELERATED_WIDGET_MAC_FULLSCREEN_LOW_POWER_COORDINATOR_H_

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"

@class CALayer;

namespace ui {

class ACCELERATED_WIDGET_MAC_EXPORT FullscreenLowPowerCoordinator {
 public:
  // This is called whenever the AcceleratedWidgetMac gets a frame, and
  // indicates to the FullscreenLowPowerCoordinator whether or not the low power
  // layer has valid content for this frame.
  virtual void SetLowPowerLayerValid(bool valid) = 0;

  // This is to be called if the AcceleratedWidget is going to be deactivated
  // or destroyed. In this call, the coordinator must call back into the
  // AccleratedWidgetMac to reset its FullscreenLowPowerCoordinator.
  virtual void WillLoseAcceleratedWidget() = 0;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_FULLSCREEN_LOW_POWER_COORDINATOR_H_
