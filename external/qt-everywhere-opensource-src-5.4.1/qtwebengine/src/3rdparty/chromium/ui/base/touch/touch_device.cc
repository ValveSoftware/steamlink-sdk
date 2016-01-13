// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

namespace ui {

// Platforms supporting touch link in an alternate implementation of this
// method.
bool IsTouchDevicePresent() {
  return false;
}

int MaxTouchPoints() {
  return 0;
}

}  // namespace ui
