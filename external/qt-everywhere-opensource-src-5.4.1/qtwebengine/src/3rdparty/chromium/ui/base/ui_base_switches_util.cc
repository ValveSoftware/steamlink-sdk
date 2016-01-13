// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches_util.h"

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"

namespace switches {

bool IsTextInputFocusManagerEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTextInputFocusManager);
}

bool IsTouchDragDropEnabled() {
#if defined(OS_CHROMEOS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTouchDragDrop);
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTouchDragDrop);
#endif
}

bool IsTouchEditingEnabled() {
#if defined(USE_AURA)
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTouchEditing);
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTouchEditing);
#endif
}

}  // namespace switches
