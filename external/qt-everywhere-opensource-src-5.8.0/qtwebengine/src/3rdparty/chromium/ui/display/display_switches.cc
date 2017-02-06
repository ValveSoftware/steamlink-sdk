// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/display/display_switches.h"

namespace switches {

// Overrides the device scale factor for the browser UI and the contents.
const char kForceDeviceScaleFactor[] = "force-device-scale-factor";

#if defined(OS_CHROMEOS)
const char kDisableDisplayColorCalibration[] =
    "disable-display-color-calibration";
#endif

}  // namespace switches
