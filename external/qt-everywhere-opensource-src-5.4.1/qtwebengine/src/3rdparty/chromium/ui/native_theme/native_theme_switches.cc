// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "ui/native_theme/native_theme_switches.h"

namespace switches {

// Enables overlay scrollbars on Aura or Linux. Does nothing on Mac.
const char kEnableOverlayScrollbar[] = "enable-overlay-scrollbar";

// Disables overlay scrollbars on Aura or Linux. Does nothing on Mac.
const char kDisableOverlayScrollbar[] = "disable-overlay-scrollbar";

}  // namespace switches

namespace ui {

bool IsOverlayScrollbarEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableOverlayScrollbar))
    return false;
  else if (command_line.HasSwitch(switches::kEnableOverlayScrollbar))
    return true;

  return false;
}

}  // namespace ui
