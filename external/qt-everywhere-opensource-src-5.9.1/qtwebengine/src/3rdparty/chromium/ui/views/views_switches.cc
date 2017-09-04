// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace views {
namespace switches {

// Please keep alphabetized.

// Specifies if a heuristic should be used to determine the most probable
// target of a gesture, where the touch region is represented by a rectangle.
const char kDisableViewsRectBasedTargeting[] =
    "disable-views-rect-based-targeting";

bool IsRectBasedTargetingEnabled() {
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableViewsRectBasedTargeting);
#else
  return false;
#endif
}

}  // namespace switches
}  // namespace views
