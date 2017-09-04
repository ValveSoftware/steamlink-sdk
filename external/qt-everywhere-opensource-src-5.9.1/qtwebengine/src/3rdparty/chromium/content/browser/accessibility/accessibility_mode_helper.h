// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MODE_HELPER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MODE_HELPER_H_

#include "content/common/accessibility_mode_enums.h"
#include "content/common/content_export.h"

namespace content {

// Returns base accessibility mode constant, depends on OS version.
CONTENT_EXPORT AccessibilityMode GetBaseAccessibilityMode();

// Adds the given accessibility mode constant to the given accessibility mode
// bitmap.
CONTENT_EXPORT AccessibilityMode
    AddAccessibilityModeTo(AccessibilityMode to, AccessibilityMode mode_to_add);

// Removes the given accessibility mode constant from the given accessibility
// mode bitmap, managing the bits that are shared with other modes such that a
// bit will only be turned off when all modes that depend on it have been
// removed.
CONTENT_EXPORT AccessibilityMode
    RemoveAccessibilityModeFrom(AccessibilityMode to,
                                AccessibilityMode mode_to_remove);

} //  namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MODE_HELPER_H_
