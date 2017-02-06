// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"

namespace content {

namespace {

AccessibilityMode CastToAccessibilityMode(unsigned int int_mode) {
  AccessibilityMode mode = static_cast<AccessibilityMode>(int_mode);
  switch (mode) {
  case AccessibilityModeOff:
  case AccessibilityModeComplete:
  case AccessibilityModeTreeOnly:
    return mode;
  }
  DCHECK(false) << "Could not convert to AccessibilityMode: " << int_mode;
  return AccessibilityModeOff;
}

}  // namespace

AccessibilityMode GetBaseAccessibilityMode() {
  AccessibilityMode accessibility_mode = AccessibilityModeOff;
  return accessibility_mode;
}

AccessibilityMode AddAccessibilityModeTo(AccessibilityMode to,
                                         AccessibilityMode mode_to_add) {
  return CastToAccessibilityMode(to | mode_to_add);
}

AccessibilityMode RemoveAccessibilityModeFrom(
    AccessibilityMode from,
    AccessibilityMode mode_to_remove) {
  unsigned int new_mode = from ^ (mode_to_remove & from);
  return CastToAccessibilityMode(new_mode);
}

}  // namespace content
