// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_DPI_SETUP_H_
#define UI_BASE_WIN_DPI_SETUP_H_

#include "ui/base/ui_base_export.h"

namespace ui {
namespace win {

// Initializes the device scale factor. If support is enabled, this will set
// the best available scale based on the screen's pixel density. This can be
// affected (overridden) by --force-device-scale-factor=x
// This function can be called only once for the lifetime of a process.
UI_BASE_EXPORT void InitDeviceScaleFactor();

}  // namespace win
}  // namespace ui

#endif  // UI_BASE_WIN_DPI_SETUP_H_
