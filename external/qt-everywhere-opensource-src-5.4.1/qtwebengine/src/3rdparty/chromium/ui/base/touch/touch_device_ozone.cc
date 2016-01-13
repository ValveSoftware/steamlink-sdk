// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

namespace ui {

bool IsTouchDevicePresent() {
  // TODO(sadrul@chromium.org): Support evdev hotplugging.
  return true;
}

int MaxTouchPoints() {
  // Hard-code this to 11 until we have a real implementation.
  return 11;
}

}  // namespace ui
