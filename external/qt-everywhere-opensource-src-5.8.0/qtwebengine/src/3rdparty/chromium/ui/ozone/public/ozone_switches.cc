// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_switches.h"

namespace switches {

// Specify ozone platform implementation to use.
const char kOzonePlatform[] = "ozone-platform";

// Specify location for image dumps.
const char kOzoneDumpFile[] = "ozone-dump-file";

// Enable support for a single overlay plane.
const char kOzoneTestSingleOverlaySupport[] =
    "ozone-test-single-overlay-support";

// Specifies the size of the primary display at initialization.
const char kOzoneInitialDisplayBounds[] = "ozone-initial-display-bounds";

// Specifies the physical display size in millimeters.
const char kOzoneInitialDisplayPhysicalSizeMm[] =
    "ozone-initial-display-physical-size-mm";

}  // namespace switches
