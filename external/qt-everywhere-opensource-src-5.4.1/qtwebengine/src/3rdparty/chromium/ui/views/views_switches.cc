// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_switches.h"

#include "base/command_line.h"

namespace views {
namespace switches {

// Please keep alphabetized.

// Specifies if a heuristic should be used to determine the most probable
// target of a gesture, where the touch region is represented by a rectangle.
const char kDisableViewsRectBasedTargeting[] =
    "disable-views-rect-based-targeting";

#if defined(USE_X11) && !defined(OS_CHROMEOS)
// When enabled, tries to get a transparent X11 visual so that we can have
// per-pixel alpha in windows.
//
// TODO(erg): Remove this switch once we've stabilized the code
// path. http://crbug.com/369209
const char kEnableTransparentVisuals[] = "enable-transparent-visuals";
#endif

bool IsRectBasedTargetingEnabled() {
#if defined(OS_CHROMEOS) || defined(OS_WIN)
  return !CommandLine::ForCurrentProcess()->
      HasSwitch(kDisableViewsRectBasedTargeting);
#else
  return false;
#endif
}

}  // namespace switches
}  // namespace views
