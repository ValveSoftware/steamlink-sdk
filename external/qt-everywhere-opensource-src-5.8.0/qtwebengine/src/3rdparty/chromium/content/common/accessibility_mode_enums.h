// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_MODE_ENUMS_H_
#define CONTENT_COMMON_ACCESSIBILITY_MODE_ENUMS_H_

// Note: keep enums in content/browser/resources/accessibility/accessibility.js
// in sync with these two enums.
enum AccessibilityModeFlag {
  // Accessibility updates are processed to create platform trees and events are
  // passed to platform APIs in the browser.
  AccessibilityModeFlagPlatform = 1 << 0,

  // Accessibility is on, and the full tree is computed. If this flag is off,
  // only limited information about editable text nodes is sent to the browser
  // process. Useful for implementing limited UIA on tablets.
  AccessibilityModeFlagFullTree = 1 << 1,
};

enum AccessibilityMode {
  // All accessibility is off.
  AccessibilityModeOff = 0,

  // Renderer accessibility is on, and platform APIs are called.
  AccessibilityModeComplete =
      AccessibilityModeFlagPlatform | AccessibilityModeFlagFullTree,

  // Renderer accessibility is on, and events are passed to any extensions
  // requesting automation, but not to platform accessibility.
  AccessibilityModeTreeOnly = AccessibilityModeFlagFullTree,
};

#endif  // CONTENT_COMMON_ACCESSIBILITY_MODE_ENUMS_H_
