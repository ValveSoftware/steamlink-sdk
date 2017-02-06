// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keyboard {

class KeyboardUtilTest : public testing::Test {
 public:
  KeyboardUtilTest() {}
  ~KeyboardUtilTest() override {}

  // Sets all flags controlling whether the keyboard should be shown to
  // their disabled state.
  void DisableAllFlags() {
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    keyboard::SetKeyboardShowOverride(
        keyboard::KEYBOARD_SHOW_OVERRIDE_DISABLED);
    keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_DISABLED);
  }

  // Sets all flags controlling whether the keyboard should be shown to
  // their enabled state.
  void EnableAllFlags() {
    keyboard::SetAccessibilityKeyboardEnabled(true);
    keyboard::SetTouchKeyboardEnabled(true);
    keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_ENABLED);
    keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_ENABLED);
  }

  // Sets all flags controlling whether the keyboard should be shown to
  // their neutral state.
  void ResetAllFlags() {
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_NONE);
    keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_AUTO);
  }

  void SetUp() override { ResetAllFlags(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardUtilTest);
};

// Tests that we respect the accessibility setting.
TEST_F(KeyboardUtilTest, AlwaysShowIfA11yEnabled) {
  // Disabled by default.
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  // If enabled by accessibility, should ignore other flag values.
  DisableAllFlags();
  keyboard::SetAccessibilityKeyboardEnabled(true);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
}

// Tests that we respect the policy setting.
TEST_F(KeyboardUtilTest, AlwaysShowIfPolicyEnabled) {
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  // If policy is enabled, should ignore other flag values.
  DisableAllFlags();
  keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_ENABLED);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
}

// Tests that we respect the policy setting.
TEST_F(KeyboardUtilTest, HidesIfPolicyDisabled) {
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  EnableAllFlags();
  // Set accessibility to neutral since accessibility has higher precedence.
  keyboard::SetAccessibilityKeyboardEnabled(false);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  // Disable policy. Keyboard should be disabled.
  keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_DISABLED);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
}

// Tests that the keyboard shows when requested state provided higher priority
// flags have not been set.
TEST_F(KeyboardUtilTest, ShowKeyboardWhenRequested) {
  DisableAllFlags();
  // Remove device policy, which has higher precedence than us.
  keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_NONE);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  // Requested should have higher precedence than all the remaining flags.
  keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_ENABLED);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
}

// Tests that the touch keyboard is hidden when requested state is disabled and
// higher priority flags have not been set.
TEST_F(KeyboardUtilTest, HideKeyboardWhenRequested) {
  EnableAllFlags();
  // Remove higher precedence flags.
  keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_NONE);
  keyboard::SetAccessibilityKeyboardEnabled(false);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  // Set requested state to disable. Keyboard should disable.
  keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_DISABLED);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
}

// SetTouchKeyboardEnabled has the lowest priority, but should still work when
// none of the other flags are enabled.
TEST_F(KeyboardUtilTest, HideKeyboardWhenTouchEnabled) {
  ResetAllFlags();
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  keyboard::SetTouchKeyboardEnabled(true);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
}

}  // namespace keyboard
