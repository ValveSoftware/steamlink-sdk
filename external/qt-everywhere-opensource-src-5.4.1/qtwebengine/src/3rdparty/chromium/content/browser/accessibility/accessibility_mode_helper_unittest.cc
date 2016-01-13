// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/common/view_message_enums.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

TEST(AccessibilityModeHelperTest, TestNoOpRemove) {
  EXPECT_EQ(AccessibilityModeComplete,
            RemoveAccessibilityModeFrom(AccessibilityModeComplete,
                                        AccessibilityModeOff));
}

TEST(AccessibilityModeHelperTest, TestRemoveSelf) {
  AccessibilityMode kOffMode = AccessibilityModeOff;
#if defined(OS_WIN)
  // Always preserve AccessibilityModeEditableTextOnly on Windows 8,
  // see RemoveAccessibilityModeFrom() implementation.
  // Test won't pass if switches::kDisableRendererAccessibility is set.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    kOffMode = AccessibilityModeEditableTextOnly;
  }
#endif  // defined(OS_WIN)

  EXPECT_EQ(kOffMode,
            RemoveAccessibilityModeFrom(AccessibilityModeComplete,
                                        AccessibilityModeComplete));

  EXPECT_EQ(
      kOffMode,
      RemoveAccessibilityModeFrom(AccessibilityModeEditableTextOnly,
                                  AccessibilityModeEditableTextOnly));
}

TEST(AccessibilityModeHelperTest, TestAddMode) {
  EXPECT_EQ(
      AccessibilityModeComplete,
      AddAccessibilityModeTo(AccessibilityModeEditableTextOnly,
                             AccessibilityModeComplete));
  EXPECT_EQ(
      AccessibilityModeComplete,
      AddAccessibilityModeTo(AccessibilityModeEditableTextOnly,
                             AccessibilityModeTreeOnly));
}

}  // namespace content
