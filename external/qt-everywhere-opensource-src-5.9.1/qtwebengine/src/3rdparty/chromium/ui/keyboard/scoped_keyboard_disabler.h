// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_SCOPED_KEYBOARD_DISABLER_H_
#define UI_KEYBOARD_SCOPED_KEYBOARD_DISABLER_H_

#include "base/macros.h"
#include "ui/keyboard/keyboard_export.h"

namespace keyboard {

// Saves the force disable keyboard state, and restores the state when going out
// of scope.
class KEYBOARD_EXPORT ScopedKeyboardDisabler {
 public:
  ScopedKeyboardDisabler();
  ~ScopedKeyboardDisabler();

  // Blocks the keyboard from showing up. This should only be used for cases
  // where the focus needs to be passed around without poping up the keyboard.
  // It needs to be turned off or this object needs to go out of scope when done
  // with, otherwise the keyboard will not pop up again.
  void SetForceDisableVirtualKeyboard(bool disable);

  // Return true if keyboard has been blocked from showing up.
  static bool GetForceDisableVirtualKeyboard();

 private:
  const bool force_disable_keyboard_state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedKeyboardDisabler);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_SCOPED_KEYBOARD_DISABLER_H_
