// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/common/view_message_enums.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(AccessibilityModeHelperTest, TestNoOpRemove) {
  EXPECT_EQ(AccessibilityModeComplete,
            RemoveAccessibilityModeFrom(AccessibilityModeComplete,
                                        AccessibilityModeOff));
}

TEST(AccessibilityModeHelperTest, TestRemoveSelf) {
  AccessibilityMode kBaseMode = GetBaseAccessibilityMode();

  EXPECT_EQ(kBaseMode,
            RemoveAccessibilityModeFrom(AccessibilityModeComplete,
                                        AccessibilityModeComplete));
}

TEST(AccessibilityModeHelperTest, TestAddMode) {
  EXPECT_EQ(AccessibilityModeComplete,
            AddAccessibilityModeTo(AccessibilityModeTreeOnly,
                                   AccessibilityModeComplete));
}

}  // namespace content
