// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TOUCH_TOUCH_ENABLED_H_
#define UI_BASE_TOUCH_TOUCH_ENABLED_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// Returns true if the touch-enabled flag is enabled, or if it is set to auto
// and a touch device is present.
UI_BASE_EXPORT bool AreTouchEventsEnabled();

}  // namespace ui

#endif  // UI_BASE_TOUCH_TOUCH_ENABLED_H_
