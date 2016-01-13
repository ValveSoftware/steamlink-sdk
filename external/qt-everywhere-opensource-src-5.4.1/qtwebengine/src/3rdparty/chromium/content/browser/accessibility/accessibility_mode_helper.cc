// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_mode_helper.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "content/public/common/content_switches.h"
#endif

namespace content {

namespace {

AccessibilityMode CastToAccessibilityMode(unsigned int int_mode) {
  AccessibilityMode mode = static_cast<AccessibilityMode>(int_mode);
  switch (mode) {
  case AccessibilityModeOff:
  case AccessibilityModeComplete:
  case AccessibilityModeEditableTextOnly:
  case AccessibilityModeTreeOnly:
    return mode;
  }
  DCHECK(false) << "Could not convert to AccessibilityMode: " << int_mode;
  return AccessibilityModeOff;
}

}  // namespace

AccessibilityMode AddAccessibilityModeTo(AccessibilityMode to,
                                         AccessibilityMode mode_to_add) {
  return CastToAccessibilityMode(to | mode_to_add);
}

AccessibilityMode RemoveAccessibilityModeFrom(
    AccessibilityMode from,
    AccessibilityMode mode_to_remove) {
  unsigned int new_mode = from ^ (mode_to_remove & from);
#if defined(OS_WIN)
  // On Windows 8, always enable accessibility for editable text controls
  // so we can show the virtual keyboard when one is enabled.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    if ((from & AccessibilityModeEditableTextOnly) ==
        AccessibilityModeEditableTextOnly)
      new_mode |= AccessibilityModeEditableTextOnly;
  }
#endif  // defined(OS_WIN)

  return CastToAccessibilityMode(new_mode);
}

}  // namespace content
