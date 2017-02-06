// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_DPI_H_
#define UI_DISPLAY_WIN_DPI_H_

#include "ui/display/display_export.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace win {

// Sets the device scale factor that will be used unless overridden on the
// command line by --force-device-scale-factor.  If this is not called, and that
// flag is not used, the scale factor used by the DIP conversion functions below
// will be that returned by GetDPIScale().
DISPLAY_EXPORT void SetDefaultDeviceScaleFactor(float scale);

// Gets the scale factor of the display. For example, if the display DPI is
// 96 then the scale factor is 1.0.  This clamps scale factors <= 1.25 to 1.0 to
// maintain previous (non-DPI-aware) behavior where only the font size was
// boosted.
DISPLAY_EXPORT float GetDPIScale();

// Returns the equivalent DPI for |device_scaling_factor|.
DISPLAY_EXPORT int GetDPIFromScalingFactor(float device_scaling_factor);

// Returns the equivalent scaling factor for |dpi|.
DISPLAY_EXPORT float GetScalingFactorFromDPI(int dpi);

// Win32's GetSystemMetrics uses pixel measures. This function calls
// GetSystemMetrics for the given |metric|, then converts the result to DIP.
DISPLAY_EXPORT int GetSystemMetricsInDIP(int metric);

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_DPI_H_
