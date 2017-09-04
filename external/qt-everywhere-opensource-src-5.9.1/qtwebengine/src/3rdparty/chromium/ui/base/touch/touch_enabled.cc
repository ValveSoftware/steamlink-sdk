// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_enabled.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_switches.h"

namespace ui {

namespace {

enum class TouchEventsStatus {
  AUTO,
  DISABLED,
  ENABLED,
};

TouchEventsStatus ComputeTouchFlagStatus() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const std::string touch_enabled_switch =
      command_line->HasSwitch(switches::kTouchEvents) ?
          command_line->GetSwitchValueASCII(switches::kTouchEvents) :
          switches::kTouchEventsAuto;

  if (touch_enabled_switch.empty() ||
      touch_enabled_switch == switches::kTouchEventsEnabled) {
    return TouchEventsStatus::ENABLED;
  }

  if (touch_enabled_switch == switches::kTouchEventsAuto)
    return TouchEventsStatus::AUTO;

  DLOG_IF(ERROR, touch_enabled_switch != switches::kTouchEventsDisabled) <<
      "Invalid --touch-events option: " << touch_enabled_switch;
  return TouchEventsStatus::DISABLED;
}

}  // namespace

bool AreTouchEventsEnabled() {
  static TouchEventsStatus touch_flag_status = ComputeTouchFlagStatus();

  // The #touch-events flag is used to force and simulate the presence or
  // absence of touch devices. Only if the flag is set to AUTO, we need to check
  // for the actual availability of touch devices.
  if (touch_flag_status == TouchEventsStatus::AUTO)
    return GetTouchScreensAvailability() == TouchScreensAvailability::ENABLED;

  return touch_flag_status == TouchEventsStatus::ENABLED;
}

}  // namespace ui

