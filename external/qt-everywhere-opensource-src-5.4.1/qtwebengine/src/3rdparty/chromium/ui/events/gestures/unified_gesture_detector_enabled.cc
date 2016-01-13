// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/events/event_switches.h"
#include "ui/events/gestures/unified_gesture_detector_enabled.h"

namespace ui {

bool IsUnifiedGestureDetectorEnabled() {
  const bool kUseUnifiedGestureDetectorByDefault = true;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const std::string unified_gd_enabled_switch =
      command_line.HasSwitch(switches::kUnifiedGestureDetector) ?
      command_line.GetSwitchValueASCII(switches::kUnifiedGestureDetector) :
      switches::kUnifiedGestureDetectorAuto;

  if (unified_gd_enabled_switch.empty() ||
      unified_gd_enabled_switch == switches::kUnifiedGestureDetectorEnabled) {
    return true;
  }

  if (unified_gd_enabled_switch == switches::kUnifiedGestureDetectorDisabled)
    return false;

  if (unified_gd_enabled_switch == switches::kUnifiedGestureDetectorAuto)
    return kUseUnifiedGestureDetectorByDefault;

  LOG(ERROR) << "Invalid --unified-gesture-detector option: "
             << unified_gd_enabled_switch;
  return false;
}

}  // namespace ui
