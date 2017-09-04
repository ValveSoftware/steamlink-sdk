// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/scoped_keyboard_disabler.h"

namespace keyboard {
namespace {

bool g_force_disable_keyboard = false;

}

ScopedKeyboardDisabler::ScopedKeyboardDisabler()
  : force_disable_keyboard_state_(g_force_disable_keyboard) {}

ScopedKeyboardDisabler::~ScopedKeyboardDisabler() {
  g_force_disable_keyboard = force_disable_keyboard_state_;
}

void ScopedKeyboardDisabler::SetForceDisableVirtualKeyboard(bool disable) {
  g_force_disable_keyboard = disable;
}

// static
bool ScopedKeyboardDisabler::GetForceDisableVirtualKeyboard() {
  return g_force_disable_keyboard;
}

}  // namespace keyboard
