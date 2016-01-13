// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_enabled.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_switches.h"

namespace ui {

bool AreTouchEventsEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const std::string touch_enabled_switch =
      command_line.HasSwitch(switches::kTouchEvents) ?
      command_line.GetSwitchValueASCII(switches::kTouchEvents) :
      switches::kTouchEventsAuto;

  if (touch_enabled_switch.empty() ||
      touch_enabled_switch == switches::kTouchEventsEnabled)
    return true;
  if (touch_enabled_switch == switches::kTouchEventsAuto)
    return IsTouchDevicePresent();
  if (touch_enabled_switch != switches::kTouchEventsDisabled)
    LOG(ERROR) << "Invalid --touch-events option: " << touch_enabled_switch;
  return false;
}

}  // namespace ui
